-- Seek a specified amount of wall-clock time -- seeking taking the playback
-- speed is taken into account.
--
-- Arguments:
-- - time: The amount of wall-clock time to seek relative to the current
--   position.
--
mp.register_script_message("wall-clock-seek",
    function (time)
        mp.command("seek " .. tonumber(time) * mp.get_property_native("speed"))
    end
)
