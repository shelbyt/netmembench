package.path = package.path ..";?.lua;test/?.lua;app/?.lua;"

local socket = require("socket")

local f_out = assert(io.open("/tmp/pktgen_loss_total.out", "w"))
local f_out0 = assert(io.open("/tmp/pktgen_loss_p0.out", "w"))
local f_out1 = assert(io.open("/tmp/pktgen_loss_p1.out", "w"))

local start_time = socket.gettime()
local now = start_time
local last_measure = now

local last_tx_0 = pktgen.portStats("0", "stats")[0]["opackets"]
local last_rx_0 = pktgen.portStats("0", "stats")[0]["ipackets"]

-- local last_tx_1 = pktgen.portStats("1", "stats")[1]["opackets"]
local last_rx_1 = pktgen.portStats("1", "stats")[1]["ipackets"]

while now - start_time < 5.0 do
    now = socket.gettime()

    local delta = now - last_measure
    if delta > 0.1 then
        local now_tx_0 = pktgen.portStats("0", "stats")[0]["opackets"]
        local now_rx_0 = pktgen.portStats("0", "stats")[0]["ipackets"]

        -- local now_tx_1 = pktgen.portStats("1", "stats")[1]["opackets"]
        local now_rx_1 = pktgen.portStats("1", "stats")[1]["ipackets"]

        local total_missed = (now_tx_0 - last_tx_0) - ((now_rx_0 - last_rx_0) + (now_rx_1 - last_rx_1))
        local missed_0 = ((now_tx_0 - last_tx_0) / 2) - (now_rx_0 - last_rx_0)
        local missed_1 = ((now_tx_0 - last_tx_0) / 2) - (now_rx_1 - last_rx_1)
        -- local missed_0 = (now_tx_0 - last_tx_0) - (now_rx_0 - last_rx_0)
        -- local missed_1 = (now_tx_1 - last_tx_1) - (now_rx_1 - last_rx_1)

        local total_rate = total_missed / delta
        local rate_0 = missed_0 / delta
        local rate_1 = missed_1 / delta

        f_out:write(string.format("%.3f,%.3f,%d,%.3f\n", (now - start_time), delta, total_missed, total_rate))
        f_out0:write(string.format("%.3f,%.3f,%d,%.3f\n", (now - start_time), delta, missed_0, rate_0))
        f_out1:write(string.format("%.3f,%.3f,%d,%.3f\n", (now - start_time), delta, missed_1, rate_1))
        -- f_out0:write(string.format("%.3f,%.3f,%d,%.3f\n", (now - start_time), delta, missed_0, rate_0))
        -- f_out1:write(string.format("%.3f,%.3f,%d,%.3f\n", (now - start_time), delta, missed_1, rate_1))

        last_tx_0 = now_tx_0
        -- last_tx_1 = now_tx_1

        last_rx_0 = now_rx_0
        last_rx_1 = now_rx_1

        last_measure = now
    end
end

f_out:close()
f_out0:close()
f_out1:close()
