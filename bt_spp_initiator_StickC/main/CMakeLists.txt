set(COMPONENT_SRCS bt_spp_initiator.c axp192.c st7735s.c fontx.c)
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
spiffs_create_partition_image(storage ../font FLASH_IN_PROJECT)
