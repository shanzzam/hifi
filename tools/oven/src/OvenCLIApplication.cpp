//
//  OvenCLIApplication.cpp
//  tools/oven/src
//
//  Created by Stephen Birarda on 2/20/18.
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "OvenCLIApplication.h"

#include <QtCore/QCommandLineParser>
#include <QtCore/QUrl>

#include <image/TextureProcessing.h>
#include <TextureBaker.h>

#include "BakerCLI.h"

static const QString CLI_INPUT_PARAMETER = "i";
static const QString CLI_OUTPUT_PARAMETER = "o";
static const QString CLI_TYPE_PARAMETER = "t";
static const QString CLI_DISABLE_TEXTURE_COMPRESSION_PARAMETER = "disable-texture-compression";

OvenCLIApplication::OvenCLIApplication(int argc, char* argv[]) :
    QCoreApplication(argc, argv)
{
    // parse the command line parameters
    QCommandLineParser parser;

    parser.addOptions({
        { CLI_INPUT_PARAMETER, "Path to file that you would like to bake.", "input" },
        { CLI_OUTPUT_PARAMETER, "Path to folder that will be used as output.", "output" },
        { CLI_TYPE_PARAMETER, "Type of asset. [model|material]"/*|js]"*/, "type" },
        { CLI_DISABLE_TEXTURE_COMPRESSION_PARAMETER, "Disable texture compression." }
    });

    parser.addHelpOption();
    parser.process(*this);

    if (parser.isSet(CLI_INPUT_PARAMETER) && parser.isSet(CLI_OUTPUT_PARAMETER)) {
        BakerCLI* cli = new BakerCLI(this);
        QUrl inputUrl(QDir::fromNativeSeparators(parser.value(CLI_INPUT_PARAMETER)));
        QUrl outputUrl(QDir::fromNativeSeparators(parser.value(CLI_OUTPUT_PARAMETER)));
        QString type = parser.isSet(CLI_TYPE_PARAMETER) ? parser.value(CLI_TYPE_PARAMETER) : QString::null;

        if (parser.isSet(CLI_DISABLE_TEXTURE_COMPRESSION_PARAMETER)) {
            qDebug() << "Disabling texture compression";
            TextureBaker::setCompressionEnabled(false);
        }

        QMetaObject::invokeMethod(cli, "bakeFile", Qt::QueuedConnection, Q_ARG(QUrl, inputUrl),
                                    Q_ARG(QString, outputUrl.toString()), Q_ARG(QString, type));
    } else {
        parser.showHelp();
        QCoreApplication::quit();
    }

}
