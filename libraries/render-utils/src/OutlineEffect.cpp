//
//  OutlineEffect.cpp
//  render-utils/src/
//
//  Created by Olivier Prat on 08/08/17.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "OutlineEffect.h"

#include "GeometryCache.h"

#include <render/FilterTask.h>
#include <render/SortTask.h>

#include "gpu/Context.h"
#include "gpu/StandardShaderLib.h"


#include "surfaceGeometry_copyDepth_frag.h"
#include "debug_deferred_buffer_vert.h"
#include "debug_deferred_buffer_frag.h"
#include "Outline_frag.h"
#include "Outline_filled_frag.h"

using namespace render;

OutlineRessources::OutlineRessources() {
}

void OutlineRessources::update(const gpu::TexturePointer& colorBuffer) {
    // If the depth buffer or size changed, we need to delete our FBOs and recreate them at the
    // new correct dimensions.
    if (_depthTexture) {
        auto newFrameSize = glm::ivec2(colorBuffer->getDimensions());
        if (_frameSize != newFrameSize) {
            _frameSize = newFrameSize;
            clear();
        }
    }
}

void OutlineRessources::clear() {
    _frameBuffer.reset();
    _depthTexture.reset();
    _idTexture.reset();
}

void OutlineRessources::allocate() {
    
    auto width = _frameSize.x;
    auto height = _frameSize.y;
    auto depthFormat = gpu::Element(gpu::SCALAR, gpu::FLOAT, gpu::DEPTH);

    _idTexture = gpu::TexturePointer(gpu::Texture::createRenderBuffer(gpu::Element::COLOR_RGBA_2, width, height));
    _depthTexture = gpu::TexturePointer(gpu::Texture::createRenderBuffer(depthFormat, width, height));
    
    _frameBuffer = gpu::FramebufferPointer(gpu::Framebuffer::create("outlineDepth"));
    _frameBuffer->setDepthStencilBuffer(_depthTexture, depthFormat);
    _frameBuffer->setRenderBuffer(0, _idTexture);
}

gpu::FramebufferPointer OutlineRessources::getFramebuffer() {
    if (!_frameBuffer) {
        allocate();
    }
    return _frameBuffer;
}

gpu::TexturePointer OutlineRessources::getDepthTexture() {
    if (!_depthTexture) {
        allocate();
    }
    return _depthTexture;
}

gpu::TexturePointer OutlineRessources::getIdTexture() {
    if (!_idTexture) {
        allocate();
    }
    return _idTexture;
}

void DrawOutlineMask::run(const render::RenderContextPointer& renderContext, const Inputs& inputs, Outputs& output) {
    assert(renderContext->args);
    assert(renderContext->args->hasViewFrustum());
    auto& inShapes = inputs.get0();
    auto& deferredFrameBuffer = inputs.get1();

    if (!inShapes.empty()) {
        RenderArgs* args = renderContext->args;
        ShapeKey::Builder defaultKeyBuilder;

        if (!_outlineRessources) {
            _outlineRessources = std::make_shared<OutlineRessources>();
        }
        _outlineRessources->update(deferredFrameBuffer->getDeferredColorTexture());

        gpu::doInBatch(args->_context, [&](gpu::Batch& batch) {
            args->_batch = &batch;

            batch.setFramebuffer(_outlineRessources->getFramebuffer());
            // Clear it
            batch.clearFramebuffer(
                gpu::Framebuffer::BUFFER_COLOR0 | gpu::Framebuffer::BUFFER_DEPTH,
                vec4(0.0f, 0.0f, 0.0f, 0.0f), 1.0f, 0, false);

            // Setup camera, projection and viewport for all items
            batch.setViewportTransform(args->_viewport);
            batch.setStateScissorRect(args->_viewport);

            glm::mat4 projMat;
            Transform viewMat;
            args->getViewFrustum().evalProjectionMatrix(projMat);
            args->getViewFrustum().evalViewTransform(viewMat);

            batch.setProjectionTransform(projMat);
            batch.setViewTransform(viewMat);

            auto maskPipeline = _shapePlumber->pickPipeline(args, defaultKeyBuilder);
            auto maskSkinnedPipeline = _shapePlumber->pickPipeline(args, defaultKeyBuilder.withSkinned());

            std::vector<ShapeKey> skinnedShapeKeys{};
            auto colorLoc = maskPipeline.get()->pipeline->getProgram()->getUniforms().findLocation("color");
            glm::vec4 idColor{ 1.0f, 0.0f, 0.0f, 0.0f };

            // Iterate through all inShapes and render the unskinned
            args->_shapePipeline = maskPipeline;
            batch.setPipeline(maskPipeline->pipeline);
            batch._glUniform4f(colorLoc, idColor.r, idColor.g, idColor.b, idColor.a);
            for (auto items : inShapes) {
                if (items.first.isSkinned()) {
                    skinnedShapeKeys.push_back(items.first);
                }
                else {
                    renderItems(renderContext, items.second);
                }
            }

            colorLoc = maskSkinnedPipeline.get()->pipeline->getProgram()->getUniforms().findLocation("color");
            // Reiterate to render the skinned
            args->_shapePipeline = maskSkinnedPipeline;
            batch.setPipeline(maskSkinnedPipeline->pipeline);
            batch._glUniform4f(colorLoc, idColor.r, idColor.g, idColor.b, idColor.a);
            for (const auto& key : skinnedShapeKeys) {
                renderItems(renderContext, inShapes.at(key));
            }

            args->_shapePipeline = nullptr;
            args->_batch = nullptr;
        });

        output = _outlineRessources;
    } else {
        output = nullptr;
    }
}

DrawOutline::DrawOutline() {
}

void DrawOutline::configure(const Config& config) {
    _color = config.color;
    _blurKernelSize = std::min(10, std::max(2, (int)floorf(config.width*2 + 0.5f)));
    // Size is in normalized screen height. We decide that for outline width = 1, this is equal to 1/400.
    _size = config.width / 400.f;
    _fillOpacityUnoccluded = config.fillOpacityUnoccluded;
    _fillOpacityOccluded = config.fillOpacityOccluded;
    _threshold = config.glow ? 1.f : 1e-3f;
    _intensity = config.intensity * (config.glow ? 2.f : 1.f);
}

void DrawOutline::run(const render::RenderContextPointer& renderContext, const Inputs& inputs) {
    auto outlineFrameBuffer = inputs.get1();

    if (outlineFrameBuffer) {
        auto sceneDepthBuffer = inputs.get2();
        const auto frameTransform = inputs.get0();
        auto outlinedDepthTexture = outlineFrameBuffer->getDepthTexture();
        auto destinationFrameBuffer = inputs.get3();
        auto framebufferSize = glm::ivec2(outlinedDepthTexture->getDimensions());

        if (!_primaryWithoutDepthBuffer || framebufferSize!=_frameBufferSize) {
            // Failing to recreate this frame buffer when the screen has been resized creates a bug on Mac
            _primaryWithoutDepthBuffer = gpu::FramebufferPointer(gpu::Framebuffer::create("primaryWithoutDepth"));
            _primaryWithoutDepthBuffer->setRenderBuffer(0, destinationFrameBuffer->getRenderBuffer(0));
            _frameBufferSize = framebufferSize;
        }

        if (sceneDepthBuffer) {
            const auto OPACITY_EPSILON = 5e-3f;
            auto pipeline = getPipeline(_fillOpacityUnoccluded>OPACITY_EPSILON || _fillOpacityOccluded>OPACITY_EPSILON);
            auto args = renderContext->args;
            {
                auto& configuration = _configuration.edit();
                configuration._color = _color;
                configuration._intensity = _intensity;
                configuration._fillOpacityUnoccluded = _fillOpacityUnoccluded;
                configuration._fillOpacityOccluded = _fillOpacityOccluded;
                configuration._threshold = _threshold;
                configuration._blurKernelSize = _blurKernelSize;
                configuration._size.x = _size * _frameBufferSize.y / _frameBufferSize.x;
                configuration._size.y = _size;
            }

            gpu::doInBatch(args->_context, [&](gpu::Batch& batch) {
                batch.enableStereo(false);
                batch.setFramebuffer(_primaryWithoutDepthBuffer);

                batch.setViewportTransform(args->_viewport);
                batch.setProjectionTransform(glm::mat4());
                batch.resetViewTransform();
                batch.setModelTransform(gpu::Framebuffer::evalSubregionTexcoordTransform(_frameBufferSize, args->_viewport));
                batch.setPipeline(pipeline);

                batch.setUniformBuffer(OUTLINE_PARAMS_SLOT, _configuration);
                batch.setUniformBuffer(FRAME_TRANSFORM_SLOT, frameTransform->getFrameTransformBuffer());
                batch.setResourceTexture(SCENE_DEPTH_SLOT, sceneDepthBuffer->getPrimaryDepthTexture());
                batch.setResourceTexture(OUTLINED_DEPTH_SLOT, outlinedDepthTexture);
                batch.draw(gpu::TRIANGLE_STRIP, 4);

                // Restore previous frame buffer
                batch.setFramebuffer(destinationFrameBuffer);
            });
        }
    }
}

const gpu::PipelinePointer& DrawOutline::getPipeline(bool isFilled) {
    if (!_pipeline) {
        auto vs = gpu::StandardShaderLib::getDrawViewportQuadTransformTexcoordVS();
        auto ps = gpu::Shader::createPixel(std::string(Outline_frag));
        gpu::ShaderPointer program = gpu::Shader::createProgram(vs, ps);

        gpu::Shader::BindingSet slotBindings;
        slotBindings.insert(gpu::Shader::Binding("outlineParamsBuffer", OUTLINE_PARAMS_SLOT));
        slotBindings.insert(gpu::Shader::Binding("deferredFrameTransformBuffer", FRAME_TRANSFORM_SLOT));
        slotBindings.insert(gpu::Shader::Binding("sceneDepthMap", SCENE_DEPTH_SLOT));
        slotBindings.insert(gpu::Shader::Binding("outlinedDepthMap", OUTLINED_DEPTH_SLOT));
        gpu::Shader::makeProgram(*program, slotBindings);

        gpu::StatePointer state = gpu::StatePointer(new gpu::State());
        state->setDepthTest(gpu::State::DepthTest(false, false));
        state->setBlendFunction(true, gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA);
        _pipeline = gpu::Pipeline::create(program, state);

        ps = gpu::Shader::createPixel(std::string(Outline_filled_frag));
        program = gpu::Shader::createProgram(vs, ps);
        gpu::Shader::makeProgram(*program, slotBindings);
        _pipelineFilled = gpu::Pipeline::create(program, state);
    }
    return isFilled ? _pipelineFilled : _pipeline;
}

DebugOutline::DebugOutline() {
    _geometryDepthId = DependencyManager::get<GeometryCache>()->allocateID();
    _geometryColorId = DependencyManager::get<GeometryCache>()->allocateID();
}

DebugOutline::~DebugOutline() {
    auto geometryCache = DependencyManager::get<GeometryCache>();
    if (geometryCache) {
        geometryCache->releaseID(_geometryDepthId);
        geometryCache->releaseID(_geometryColorId);
    }
}

void DebugOutline::configure(const Config& config) {
    _isDisplayEnabled = config.viewMask;
}

void DebugOutline::run(const render::RenderContextPointer& renderContext, const Inputs& input) {
    const auto outlineFramebuffer = input;

    if (_isDisplayEnabled && outlineFramebuffer) {
        assert(renderContext->args);
        assert(renderContext->args->hasViewFrustum());
        RenderArgs* args = renderContext->args;

        gpu::doInBatch(args->_context, [&](gpu::Batch& batch) {
            batch.enableStereo(false);
            batch.setViewportTransform(args->_viewport);

            const auto geometryBuffer = DependencyManager::get<GeometryCache>();

            glm::mat4 projMat;
            Transform viewMat;
            args->getViewFrustum().evalProjectionMatrix(projMat);
            args->getViewFrustum().evalViewTransform(viewMat);
            batch.setProjectionTransform(projMat);
            batch.setViewTransform(viewMat, true);
            batch.setModelTransform(Transform());

            const glm::vec4 color(1.0f, 1.0f, 1.0f, 1.0f);

            batch.setPipeline(getDepthPipeline());
            batch.setResourceTexture(0, outlineFramebuffer->getDepthTexture());
            {
                const glm::vec2 bottomLeft(-1.0f, -1.0f);
                const glm::vec2 topRight(0.0f, 1.0f);
                geometryBuffer->renderQuad(batch, bottomLeft, topRight, color, _geometryDepthId);
            }

            batch.setPipeline(getIdPipeline());
            batch.setResourceTexture(0, outlineFramebuffer->getIdTexture());
            {
                const glm::vec2 bottomLeft(0.0f, -1.0f);
                const glm::vec2 topRight(1.0f, 1.0f);
                geometryBuffer->renderQuad(batch, bottomLeft, topRight, color, _geometryColorId);
            }

            batch.setResourceTexture(0, nullptr);
        });
    }
}

void DebugOutline::initializePipelines() {
    static const std::string VERTEX_SHADER{ debug_deferred_buffer_vert };
    static const std::string FRAGMENT_SHADER{ debug_deferred_buffer_frag };
    static const std::string SOURCE_PLACEHOLDER{ "//SOURCE_PLACEHOLDER" };
    static const auto SOURCE_PLACEHOLDER_INDEX = FRAGMENT_SHADER.find(SOURCE_PLACEHOLDER);
    Q_ASSERT_X(SOURCE_PLACEHOLDER_INDEX != std::string::npos, Q_FUNC_INFO,
               "Could not find source placeholder");

    auto state = std::make_shared<gpu::State>();
    state->setDepthTest(gpu::State::DepthTest(false));

    const auto vs = gpu::Shader::createVertex(VERTEX_SHADER);

    // Depth shader
    {
        static const std::string DEPTH_SHADER{
            "vec4 getFragmentColor() {"
            "   float Zdb = texelFetch(depthMap, ivec2(gl_FragCoord.xy), 0).x;"
            "   Zdb = 1.0-(1.0-Zdb)*100;"
            "   return vec4(Zdb, Zdb, Zdb, 1.0); "
            "}"
        };

        auto fragmentShader = FRAGMENT_SHADER;
        fragmentShader.replace(SOURCE_PLACEHOLDER_INDEX, SOURCE_PLACEHOLDER.size(), DEPTH_SHADER);

        const auto ps = gpu::Shader::createPixel(fragmentShader);
        const auto program = gpu::Shader::createProgram(vs, ps);

        gpu::Shader::BindingSet slotBindings;
        slotBindings.insert(gpu::Shader::Binding("depthMap", 0));
        gpu::Shader::makeProgram(*program, slotBindings);

        _depthPipeline = gpu::Pipeline::create(program, state);
    }

    // ID shader
    {
        static const std::string ID_SHADER{
            "vec4 getFragmentColor() {"
            "   return texelFetch(albedoMap, ivec2(gl_FragCoord.xy), 0); "
            "}"
        };

        auto fragmentShader = FRAGMENT_SHADER;
        fragmentShader.replace(SOURCE_PLACEHOLDER_INDEX, SOURCE_PLACEHOLDER.size(), ID_SHADER);

        const auto ps = gpu::Shader::createPixel(fragmentShader);
        const auto program = gpu::Shader::createProgram(vs, ps);

        gpu::Shader::BindingSet slotBindings;
        slotBindings.insert(gpu::Shader::Binding("albedoMap", 0));
        gpu::Shader::makeProgram(*program, slotBindings);

        _idPipeline = gpu::Pipeline::create(program, state);
    }
}

const gpu::PipelinePointer& DebugOutline::getDepthPipeline() {
    if (!_depthPipeline) {
        initializePipelines();
    }

    return _depthPipeline;
}

const gpu::PipelinePointer& DebugOutline::getIdPipeline() {
    if (!_idPipeline) {
        initializePipelines();
    }

    return _idPipeline;
}

DrawOutlineTask::DrawOutlineTask() {

}

void DrawOutlineTask::configure(const Config& config) {

}

void DrawOutlineTask::build(JobModel& task, const render::Varying& inputs, render::Varying& outputs) {
    const auto groups = inputs.getN<Inputs>(0).get<Groups>();
    const auto selectedMetas = groups[0];
    const auto sceneFrameBuffer = inputs.getN<Inputs>(1);
    const auto primaryFramebuffer = inputs.getN<Inputs>(2);
    const auto deferredFrameTransform = inputs.getN<Inputs>(3);

    // Prepare the ShapePipeline
    ShapePlumberPointer shapePlumber = std::make_shared<ShapePlumber>();
    {
        auto state = std::make_shared<gpu::State>();
        state->setDepthTest(true, true, gpu::LESS_EQUAL);
        state->setColorWriteMask(true, true, true, true);

        initMaskPipelines(*shapePlumber, state);
    }

    const auto outlinedItemIDs = task.addJob<render::MetaToSubItems>("OutlineMetaToSubItemIDs", selectedMetas);
    const auto outlinedItems = task.addJob<render::IDsToBounds>("OutlineMetaToSubItems", outlinedItemIDs, true);

    // Sort
    const auto sortedPipelines = task.addJob<render::PipelineSortShapes>("OutlinePipelineSort", outlinedItems);
    const auto sortedShapes = task.addJob<render::DepthSortShapes>("OutlineDepthSort", sortedPipelines);

    // Draw depth of outlined objects in separate buffer
    const auto drawMaskInputs = DrawOutlineMask::Inputs(sortedShapes, sceneFrameBuffer).asVarying();
    const auto outlinedFrameBuffer = task.addJob<DrawOutlineMask>("OutlineMask", drawMaskInputs, shapePlumber);

    // Draw outline
    const auto drawOutlineInputs = DrawOutline::Inputs(deferredFrameTransform, outlinedFrameBuffer, sceneFrameBuffer, primaryFramebuffer).asVarying();
    task.addJob<DrawOutline>("OutlineEffect", drawOutlineInputs);

    // Debug outline
    task.addJob<DebugOutline>("OutlineDebug", outlinedFrameBuffer);
}

#include "model_shadow_vert.h"
#include "model_shadow_fade_vert.h"
#include "skin_model_shadow_vert.h"
#include "skin_model_shadow_fade_vert.h"

#include "model_outline_frag.h"
#include "model_outline_fade_frag.h"

void DrawOutlineTask::initMaskPipelines(render::ShapePlumber& shapePlumber, gpu::StatePointer state) {
    auto modelVertex = gpu::Shader::createVertex(std::string(model_shadow_vert));
    auto modelPixel = gpu::Shader::createPixel(std::string(model_outline_frag));
    gpu::ShaderPointer modelProgram = gpu::Shader::createProgram(modelVertex, modelPixel);
    shapePlumber.addPipeline(
        ShapeKey::Filter::Builder().withoutSkinned().withoutFade(),
        modelProgram, state);

    auto skinVertex = gpu::Shader::createVertex(std::string(skin_model_shadow_vert));
    gpu::ShaderPointer skinProgram = gpu::Shader::createProgram(skinVertex, modelPixel);
    shapePlumber.addPipeline(
        ShapeKey::Filter::Builder().withSkinned().withoutFade(),
        skinProgram, state);

    auto modelFadeVertex = gpu::Shader::createVertex(std::string(model_shadow_fade_vert));
    auto modelFadePixel = gpu::Shader::createPixel(std::string(model_outline_fade_frag));
    gpu::ShaderPointer modelFadeProgram = gpu::Shader::createProgram(modelFadeVertex, modelFadePixel);
    shapePlumber.addPipeline(
        ShapeKey::Filter::Builder().withoutSkinned().withFade(),
        modelFadeProgram, state);

    auto skinFadeVertex = gpu::Shader::createVertex(std::string(skin_model_shadow_fade_vert));
    gpu::ShaderPointer skinFadeProgram = gpu::Shader::createProgram(skinFadeVertex, modelFadePixel);
    shapePlumber.addPipeline(
        ShapeKey::Filter::Builder().withSkinned().withFade(),
        skinFadeProgram, state);
}
