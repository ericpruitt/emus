-- This observer prevents the playback speed from ever being set to the minimum
-- value of 0.01 because reaching that value causes discontinuities when using
-- relative volume changes. For example, if the adjustments are made in
-- increments of 0.25, then decreasing the speed might result in values of
-- 1.00, 0.75, ..., 0.25 and 0.01. From there, increasing it produces 0.26,
-- 0.51, ..., 1.01 rather than 0.25, ..., 1.00 like the user might expect.
-- Ensuring the true minimum value is never reached resolves this issue. This
-- hook works by detecting when the speed is set to a value less than or equal
-- to 0.01 and reverting the speed to the previous value. If a freshly loaded
-- file has a saved playback speed that is at or below this threshold, the
-- playback speed is set to 1x.
mp.observe_property("speed", "native",
    (function ()
        local previous_value = nil

        return function (_, value)
            if value <= 0.01 then
                if previous_value == nil or previous_value <= 0.01 then
                    value = 1
                else
                    value = previous_value
                end

                mp.set_property("speed", value)
            end

            -- Only show the playback speed when loading a file if it's not the
            -- default speed of 1x.
            if previous_value ~= nil or value ~= 1 then
                mp.osd_message(string.format("Speed: %.2f", value))
            end

            previous_value = value
        end
    end)()
)
