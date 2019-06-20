//
//  Created by Bradley Austin Davis 2015/06/19
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AvatarInputs_h
#define hifi_AvatarInputs_h

#include <QQuickItem>
#include <OffscreenUi.h>

#define AI_PROPERTY(type, name, initialValue) \
    Q_PROPERTY(type name READ name NOTIFY name##Changed) \
public: \
    type name() { return _##name; }; \
private: \
    type _##name{ initialValue };

class AvatarInputs : public QObject {
    Q_OBJECT
    HIFI_QML_DECL

    /**jsdoc 
     * API to help manage your Avatar's input
     * @namespace AvatarInputs
     *
     * @hifi-interface
     * @hifi-client-entity
     * @hifi-avatar
     *
     * @property {boolean} cameraEnabled <em>Read-only.</em>
     * @property {boolean} cameraMuted <em>Read-only.</em>
     * @property {boolean} isHMD <em>Read-only.</em>
     * @property {boolean} showAudioTools
     * @property {boolean} showBubbleTools
     */

    AI_PROPERTY(bool, cameraEnabled, false)
    AI_PROPERTY(bool, cameraMuted, false)
    AI_PROPERTY(bool, isHMD, false)

    Q_PROPERTY(bool showAudioTools READ showAudioTools WRITE setShowAudioTools NOTIFY showAudioToolsChanged)
    Q_PROPERTY(bool showBubbleTools READ showBubbleTools WRITE setShowBubbleTools NOTIFY showBubbleToolsChanged)
    Q_PROPERTY(bool ignoreRadiusEnabled READ getIgnoreRadiusEnabled NOTIFY ignoreRadiusEnabledChanged)
    //Q_PROPERTY(bool enteredIgnoreRadius READ getEnteredIgnoreRadius NOTIFY enteredIgnoreRadiusChanged)

public:
    static AvatarInputs* getInstance();

    /**jsdoc
     * @function AvatarInputs.loudnessToAudioLevel
     * @param {number} loudness
     * @returns {number}
     */
    Q_INVOKABLE float loudnessToAudioLevel(float loudness);

    AvatarInputs(QObject* parent = nullptr);
    void update();
    bool showAudioTools() const { return _showAudioTools; }
    bool showBubbleTools() const { return _showBubbleTools; }
    bool getIgnoreRadiusEnabled() const;
    //bool getEnteredIgnoreRadius() const;

public slots:

    /**jsdoc
     * @function AvatarInputs.setShowAudioTools
     * @param {boolean} showAudioTools
     */
    void setShowAudioTools(bool showAudioTools);

    /**jsdoc
     * @function AvatarInputs.setShowBubbleTools
     * @param {boolean} showBubbleTools
     */
    void setShowBubbleTools(bool showBubbleTools);

signals:

    /**jsdoc
     * @function AvatarInputs.cameraEnabledChanged
     * @returns {Signal}
     */
    void cameraEnabledChanged();

    /**jsdoc
     * @function AvatarInputs.cameraMutedChanged
     * @returns {Signal}
     */
    void cameraMutedChanged();

    /**jsdoc
     * @function AvatarInputs.isHMDChanged
     * @returns {Signal}
     */

    void isHMDChanged();

    /**jsdoc
     * @function AvatarInputs.showAudioToolsChanged
     * @param {boolean} show
     * @returns {Signal}
     */
    void showAudioToolsChanged(bool show);

    /**jsdoc
     * @function AvatarInputs.showBubbleToolsChanged
     * @param {boolean} show
     * @returns {Signal}
     */
    void showBubbleToolsChanged(bool show);

    /**jsdoc
     * @function AvatarInputs.avatarEnteredIgnoreRadius
     * @param {QUuid} avatarID
     * @returns {Signal}
    */
    void avatarEnteredIgnoreRadius(QUuid avatarID);

    /**jsdoc
     * @function AvatarInputs.avatarLeftIgnoreRadius
     * @param {QUuid} avatarID
     * @returns {Signal}
    */
    void avatarLeftIgnoreRadius(QUuid avatarID);

    /**jsdoc
     * @function AvatarInputs.ignoreRadiusEnabledChanged
     * @param {boolean} enabled
     * @returns {Signal}
     */
    void ignoreRadiusEnabledChanged(bool enabled);

    /**jsdoc
     * @function AvatarInputs.enteredIgnoreRadiusChanged
     * @param {boolean} enabled
     * @returns {Signal}
     */
    void enteredIgnoreRadiusChanged();

protected:

    /**jsdoc
     * @function AvatarInputs.resetSensors
     */
    Q_INVOKABLE void resetSensors();

    /**jsdoc
     * @function AvatarInputs.toggleCameraMute
     */
    Q_INVOKABLE void toggleCameraMute();

private: 
    void onAvatarEnteredIgnoreRadius();
    void onAvatarLeftIgnoreRadius();
    float _trailingAudioLoudness{ 0 };
    bool _showAudioTools { false };
    bool _showBubbleTools{ false };
};

#endif // hifi_AvatarInputs_h
