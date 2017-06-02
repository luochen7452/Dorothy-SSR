Dorothy!
CircleButtonView = require "UI.View.Control.Basic.CircleButton"

-- [signals]
-- "Tapped",(button)->
-- [params]
-- text, x, y, radius, fontName=18, fontSize=NotoSansHans-Regular
Class CircleButtonView,
	__init:(args)=>
		@_text = @label.text if @label
		@slot "TapFilter", (touch)->
			touch.enabled = false unless touch.id == 0
		@slot "Tapped",->
			enabled = @touchEnabled
			@touchEnabled = false
			@schedule once ->
				sleep!
				sleep!
				@touchEnabled = enabled

	text:property => @_text,
		(value)=>
			@_text = value
			@label.text = value if @label