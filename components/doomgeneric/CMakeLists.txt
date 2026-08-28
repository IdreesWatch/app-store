# DOOM Generic Engine - ESP-IDF Component
# Port of ozkl/doomgeneric for the Stratus Watch

set(DOOM_SRCS
    "am_map.c"
    "d_event.c"
    "d_items.c"
    "d_iwad.c"
    "d_loop.c"
    "d_main.c"
    "d_mode.c"
    "d_net.c"
    "doomdef.c"
    "doomgeneric.c"
    "doomstat.c"
    "dstrings.c"
    "dummy.c"
    "f_finale.c"
    "f_wipe.c"
    "g_game.c"
    "hu_lib.c"
    "hu_stuff.c"
    "i_cdmus.c"
    "i_endoom.c"
    "i_input.c"
    "i_joystick.c"
    "i_scale.c"
    "i_sound.c"
    "i_system.c"
    "i_timer.c"
    "i_video.c"
    "info.c"
    "m_argv.c"
    "m_bbox.c"
    "m_cheat.c"
    "m_config.c"
    "m_controls.c"
    "m_fixed.c"
    "m_menu.c"
    "m_misc.c"
    "m_random.c"
    "memio.c"
    "p_ceilng.c"
    "p_doors.c"
    "p_enemy.c"
    "p_floor.c"
    "p_inter.c"
    "p_lights.c"
    "p_map.c"
    "p_maputl.c"
    "p_mobj.c"
    "p_plats.c"
    "p_pspr.c"
    "p_saveg.c"
    "p_setup.c"
    "p_sight.c"
    "p_spec.c"
    "p_switch.c"
    "p_telept.c"
    "p_tick.c"
    "p_user.c"
    "r_bsp.c"
    "r_data.c"
    "r_draw.c"
    "r_main.c"
    "r_plane.c"
    "r_segs.c"
    "r_sky.c"
    "r_things.c"
    "s_sound.c"
    "sha1.c"
    "sounds.c"
    "st_lib.c"
    "st_stuff.c"
    "statdump.c"
    "tables.c"
    "v_video.c"
    "w_checksum.c"
    "w_file.c"
    "w_file_stdc.c"
    "w_main.c"
    "w_wad.c"
    "wi_stuff.c"
    "z_zone.c"
    "doomgeneric_stratus.c"
)

idf_component_register(
    SRCS ${DOOM_SRCS}
    INCLUDE_DIRS "."
    REQUIRES freertos esp_timer
)

# Force all DOOM BSS variables into PSRAM to prevent DRAM overflow
idf_component_get_property(doomgeneric_lib ${COMPONENT_NAME} COMPONENT_LIB)
idf_component_set_property(${COMPONENT_NAME} LDFRAGMENTS "linker.lf")

# Keep Doom at its native 320x200 output.  The previous 160x100 setting was
# invalid for this port: i_video.c uses integer scaling against the engine's
# fixed 320x200 framebuffer, so it computed a zero scale and copied no pixels.
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    DOOMGENERIC_RESX=320
    DOOMGENERIC_RESY=200
    ESP_PLATFORM=1
)

# Suppress warnings from vintage C code
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-all
    -Wno-extra
    -Wno-incompatible-pointer-types
    -Wno-int-conversion
    -Wno-pointer-sign
    -Wno-implicit-function-declaration
    -Wno-format
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-unused-function
    -Wno-missing-braces
    -Wno-sign-compare
    -Wno-char-subscripts
    -Wno-discarded-qualifiers
    -Wno-write-strings
)
