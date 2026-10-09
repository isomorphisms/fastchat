-- Actual Icky Lua policy. Bytes are embedded unchanged and parsed by the
-- pinned Icky Lua runtime, after the working C checkpoint.
local follows_stored_prefixes ← false
local barrier_interval_ns ← 200000000
local short_frames ← {
  "data: [START]\n\n",
  "data: {\"text\":\"A local response, stored before presentation.\\n\\n\"}\n\n",
  "data: {\"text\":\"UTF-8: \\u20ac \\ud83d\\ude42. Markdown is plain readable text:\\n\"}\n\n",
  "data: {\"text\":\"**disk authority**\\n```text\\nbounded windows\\n```\\n\"}\n\n",
  "data: [DONE]\n\n"
}
local composer_action ← ƒ(phase, running)
  if running then return "cancel" end
  if phase ≟ "uncertain delivery" then return "retry" end
  if phase ≟ "idle" or phase ≟ "completed" or
     phase ≟ "cancelled" or phase ≟ "failed" then return "submit" end
  return "wait"
end
local barrier_due ← ƒ(phase, elapsed_ns, pending_bytes)
  return (phase ≟ "generating" or phase ≟ "cancel pending") and
         pending_bytes > 0 and elapsed_ns ≥ barrier_interval_ns
end
local fixture_scenario ← ƒ(prompt, retrying)
  if prompt ≟ ":long" then return 1 end
  if prompt ≟ ":lost" and not retrying then return 2 end
  if prompt ≟ ":fail" then return 3 end
  return 0
end
local fixture_frame ← ƒ(step, scenario)
  if scenario ≟ 1 then
    if step ≟ 0 then return "wire", short_frames[1], false end
    if step ≥ 1 and step ≤ 2048 then return "repeat", "a", false end
    if step ≟ 2049 then return "wire", short_frames[5], true end
  elseif step ≥ 0 and step ≤ 4 then
    if step ≟ 4 and scenario ≟ 2 then return "wire", "data: [LOST]\n\n", true end
    if step ≟ 4 and scenario ≟ 3 then return "wire", "data: [FAILED]\n\n", true end
    return "wire", short_frames[step + 1], step ≟ 4
  end
  return nil
end
return {
  version ← 1,
  follows_stored_prefixes ← follows_stored_prefixes,
  composer_action ← composer_action,
  barrier_due ← barrier_due,
  fixture_scenario ← fixture_scenario,
  fixture_frame ← fixture_frame
}
