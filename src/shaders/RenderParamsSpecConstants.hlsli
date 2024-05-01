#pragma once

[[vk::constant_id(0)]] const uint rpOtherModeL = 0;
[[vk::constant_id(1)]] const uint rpOtherModeH = 0;
[[vk::constant_id(2)]] const uint rpColorCombinerL = 0;
[[vk::constant_id(3)]] const uint rpColorCombinerH = 0;
[[vk::constant_id(4)]] const uint rpFlagsValue = 0;

RenderParams getRenderParams() {
    RenderParams rp;
    rp.omL = rpOtherModeL;
    rp.omH = rpOtherModeH;
    rp.ccL = rpColorCombinerL;
    rp.ccH = rpColorCombinerH;
    rp.flags = rpFlagsValue;
    return rp;
}