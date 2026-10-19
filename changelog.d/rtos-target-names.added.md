**QNX, VxWorks, FreeRTOS and Zephyr as named targets.** `ADL_TARGET_PLATFORM`
names the platform of a cross build. QNX has code that has never been compiled
(refused unless `ADL_ACCEPT_UNBUILT_PLATFORM` is on); the other three are
designs only, and a build for them says so.
