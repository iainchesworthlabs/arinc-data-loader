**QNX, VxWorks, FreeRTOS and Zephyr as named targets.** `ADL_TARGET_PLATFORM`
names the platform of a cross build. QNX has code that has never been compiled
(refused unless `ADL_ACCEPT_UNBUILT_PLATFORM` is on); VxWorks is a design only,
and a build for it says so. (FreeRTOS and Zephyr began as designs and have since
been written; see the first entry.)
