//
//  colorSelectorEntityScript.js
//  examples/toybox/entityScripts
//
//  Created by Eric Levin on 9/21/15.
//  Copyright 2015 High Fidelity, Inc.
//

//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
/*global print, MyAvatar, Entities, AnimationCache, SoundCache, Scene, Camera, Overlays, Audio, HMD, AvatarList, AvatarManager, Controller, UndoStack, Window, Account, GlobalServices, Script, ScriptDiscoveryService, LODManager, Menu, Vec3, Quat, AudioDevice, Paths, Clipboard, Settings, XMLHttpRequest, randFloat, randInt */
/*global ColorSelector */

(function() {

    var _this;
    ColorSelector = function() {
        _this = this;
    };

    ColorSelector.prototype = {

        startFarGrabNonColliding: function() {
            this.selectColor();
        },

        clickReleaseOnEntity: function() {
            this.selectColor();
        },

        selectColor: function() {
            print("COLOR SELECTED");
        },

        preload: function(entityID) {
            this.entityID = entityID;
            var props = Entities.getEntityProperties(this.entityID, ["position, color, userData"]);
            this.position = props.position;
            this.color = props.color;
            this.whiteboard = JSON.parse(this.userData).whiteboard;
        },

    };

    // entity scripts always need to return a newly constructed object of our type
    return new ColorSelector();
});