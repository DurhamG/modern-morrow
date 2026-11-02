HUD Settings
############

.. omw-setting::
   :title: crosshair
   :type: boolean
   :range: true, false
   :default: true
   :location: :bdg-info:`In Game > Options > Prefs`

   Determines whether the crosshair or reticle is displayed.
   Enabling provides immediate feedback about the focused object.
   Disabling may improve immersion or be preferred for screenshots.

   The complete GUI, including crosshair, can be toggled at runtime with F11.

.. omw-setting::
   :title: mouse emulation cursor
   :type: boolean
   :range: true, false
   :default: true
   :location: :bdg-info:`In Game > Options > Prefs`

   Determines whether the mouse emulation cursor is displayed.
   Enabling adds a software cursor when a controller is detected.
   This addresses an issue on certain devices when the hardware cursor is not visible.

.. omw-setting::
   :title: size
   :type: float32
   :range: 0.25 to 4.0
   :default: 1.0

   The size of the HUD relative to the screen height.
   At 1.0 the HUD is laid out as if the screen were 720 pixels tall, then scaled to fill the real screen,
   so it takes up the same share of the screen at any resolution.
   Larger values make the HUD bigger.
   The GUI scaling factor does not affect the HUD.
