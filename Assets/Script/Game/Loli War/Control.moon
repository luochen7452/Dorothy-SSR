Dorothy builtin.Platformer

HPWheel = require "UI.Control.HPWheel"
AlignNode = require "UI.Control.Basic.AlignNode"
LeftTouchPad = require "UI.View.LeftTouchPad"
RightTouchPad = require "UI.View.RightTouchPad"
RestartPad = require "UI.View.RestartPad"
StartPanel = require "UI.Control.StartPanel"

{store:Store} = Data
{:GroupPlayer} = Store

playerGroup = Group {"hero"}
updatePlayerControl = (key,flag)->
	playerGroup\each => @[key] = flag if @group == GroupPlayer

root = with AlignNode isRoot:true
	\addChild HPWheel!
	switch App.platform
		when "iOS","Android"
			\addChild with LeftTouchPad!
				\slot "KeyLeftUp",-> updatePlayerControl "keyLeft", false
				\slot "KeyLeftDown",-> updatePlayerControl "keyLeft", true
				\slot "KeyRightUp",-> updatePlayerControl "keyRight", false
				\slot "KeyRightDown",-> updatePlayerControl "keyRight", true
			\addChild with RightTouchPad!
				\slot "KeyFUp",-> updatePlayerControl "keyF", false
				\slot "KeyFDown",-> updatePlayerControl "keyF", true
				\slot "KeyUpUp",-> updatePlayerControl "keyUp", false
				\slot "KeyUpDown",-> updatePlayerControl "keyUp", true
		when "macOS","Windows"
			Director.entry\addChild with Node!
				\schedule ->
					updatePlayerControl "keyLeft", Keyboard\isKeyPressed "A"
					updatePlayerControl "keyRight", Keyboard\isKeyPressed "D"
					updatePlayerControl "keyUp", Keyboard\isKeyPressed "K"
					updatePlayerControl "keyF", Keyboard\isKeyPressed "J"
	\addChild with RestartPad!
		\slot "Tapped",->
			Store.winner = -1
			root\addChild StartPanel!
			root\alignLayout!
	\addChild StartPanel!
	\addTo Director.ui
