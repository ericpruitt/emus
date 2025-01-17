-- Toggles the on screen controller between being always on and either
-- automatic mode or completely hidden depending on what the initial visibility
-- value was. If this is used in conjunction with "osc-visibility cycle", the
-- uncorrelated state used by the two messages may result in neither behaving
-- as expected.
mp.register_script_message("osc-toggle",
    (function ()
        local cursor
        local states

        local osc_conf = {
            visibility = "auto"
        }

        (require "mp.options").read_options(osc_conf, "osc")

        if osc_conf.visibility == "auto" then
            states = {"always", "auto"}
        else
            states = {"always", "never"}
        end

        cursor = states[2] == osc_conf.visibility and 2 or 1

        return function ()
            cursor = cursor == 1 and 2 or 1
            mp.commandv(
                "script-message", "osc-visibility", states[cursor], "no-osd"
            )
        end
    end)()
)
