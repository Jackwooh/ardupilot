--[[
 support precision landing on quadplanes

 This is a very simple implementation intended to act as a framework
 for development of a custom solution
--]]

local PARAM_TABLE_KEY = 12
local PARAM_TABLE_PREFIX = "PLND_"

local MAV_SEVERITY = {EMERGENCY=0, ALERT=1, CRITICAL=2, ERROR=3, WARNING=4, NOTICE=5, INFO=6, DEBUG=7}

local MODE_QLAND = 20

-- bind a parameter to a variable
function bind_param(name)
   return Parameter(name)
end

-- add a parameter and bind it to a variable
function bind_add_param(name, idx, default_value)
   assert(param:add_param(PARAM_TABLE_KEY, idx, name, default_value), string.format('could not add param %s', name))
   return bind_param(PARAM_TABLE_PREFIX .. name)
end

-- setup precland specific parameters
assert(param:add_table(PARAM_TABLE_KEY, PARAM_TABLE_PREFIX, 10), 'could not add param table')

--[[
  // @Param: PLND_ALT_CUTOFF
  // @DisplayName: Precland altitude cutoff
  // @Description: The altitude (rangefinder distance) below which we stop using the precision landing sensor and continue landing
  // @Range: 0 20
  // @Units: m
  // @User: Standard
--]]
PLND_ALT_CUTOFF = bind_add_param('ALT_CUTOFF', 1, 0)

--[[
  // @Param: DIST_CUTOFF
  // @DisplayName: Precland distance cutoff
  // @Description: The distance from target beyond which the target is ignored
  // @Range: 0 100
  // @Units: m
  // @User: Standard
--]]
PLND_DIST_CUTOFF = bind_add_param('DIST_CUTOFF', 2, 0)

-- other parameters
PLND_ENABLED = bind_param("PLND_ENABLED")
PLND_XY_DIST_MAX = bind_param("PLND_XY_DIST_MAX")
PLND_OPTIONS = bind_param("PLND_OPTIONS")

if PLND_ENABLED:get() == 0 then
   gcs:send_text(MAV_SEVERITY.INFO, "PLND: Disabled")
   return
end

local have_target = false

local rangefinder_orient = 25 -- downward

--[[
   update the have_target variable
--]]
function update_target()
   if not precland:healthy() then
      have_target = false
      return
   end
   local ok = precland:target_acquired()

   if PLND_ALT_CUTOFF:get() > 0 then
      -- require rangefinder as well
      if not rangefinder:has_data_orient(rangefinder_orient) then
         ok = false
      end
   end
   
   if ok ~= have_target then
      have_target = ok
      if have_target then
         gcs:send_text(MAV_SEVERITY.INFO, "PLND: Target Acquired")
      else
         gcs:send_text(MAV_SEVERITY.INFO, "PLND: Target Lost")
      end
   end
end

--[[
   return true if we are in an automatic landing and should
   try to use precision landing
--]]
function in_auto_land()
   return quadplane:in_vtol_land_descent() or vehicle:get_mode() == MODE_QLAND
end

-- main update function
function update()
   if PLND_ENABLED:get() < 1 then
      return
   end

   --[[
      get the current navigation target. Note that we must get this
      before we check if we are in a landing descent to prevent a race condition
      with vehicle:update_target_location()
   --]]
   local next_WP = vehicle:get_target_location()
   if not next_WP then
      -- not in a flight mode with a target location
      return
   end

   -- see if we are landing
   if not in_auto_land() then
      return
   end

   update_target()
   if not have_target then
      return
   end

   --[[ ask precland for the target. Note that we ignore the altitude
      in the return as it is unreliable
   --]]
   local loc = precland:get_target_location()
   if not loc then
      return
   end

   --[[ get rangefinder distance, and if PLND_ALT_CUTOFF is set then
      stop precland operation if below the cutoff
   --]]
   local rngfnd_distance_m = rangefinder:distance_cm_orient(rangefinder_orient) * 0.01
   if PLND_ALT_CUTOFF:get() > 0 and rngfnd_distance_m < PLND_ALT_CUTOFF:get() then
      return
   end
   
   --[[
      update the vehicle target to match the precland target
   --]]
   local new_WP = next_WP:copy()
   new_WP:lat(loc:lat())
   new_WP:lng(loc:lng())
   vehicle:update_target_location(next_WP, new_WP)

   veh_loc = ahrs:get_location()

   local xy_dist = veh_loc:get_distance(new_WP)

   --[[
      get target velocity and if velocity matching is enabled in
      PLND_OPTIONS then ask vehicle to match
   --]]
   local target_vel = precland:get_target_velocity()
   if target_vel and (PLND_OPTIONS:get():toint() & 1) ~= 0 then
      vehicle:set_velocity_match(target_vel)
   end
   if not target_vel then
      target_vel = Vector2f()
   end

   --[[
      log the target and distance
   --]]
   logger.write("PPLD", 'Lat,Lon,Alt,HDist,TDist,RFND,VN,VE',
                'LLffffff',
                'DUmmmmmm',
                'GG------',
                new_WP:lat(),
                new_WP:lng(),
                new_WP:alt(),
                xy_dist,
                next_WP:get_distance(next_WP),
                rngfnd_distance_m,
                target_vel:x(),
                target_vel:y())
   
   --[[
      stop using precland if too far away
   --]]
   if PLND_DIST_CUTOFF:get() > 0 and xy_dist > PLND_DIST_CUTOFF:get() then
      return
   end

   if xy_dist > PLND_XY_DIST_MAX:get() then
      -- pause descent till we are within the given radius
      vehicle:set_land_descent_rate(0)
   end
end

gcs:send_text(MAV_SEVERITY.INFO, "PLND: Loaded")

-- wrapper around update(). This calls update() at 20Hz,
-- and if update faults then an error is displayed, but the script is not
-- stopped
function protected_wrapper()
  local success, err = pcall(update)
  if not success then
     gcs:send_text(0, "Internal Error: " .. err)
     -- when we fault we run the update function again after 1s, slowing it
     -- down a bit so we don't flood the console with errors
     return protected_wrapper, 1000
  end
  return protected_wrapper, 100
end

-- start running update loop
return protected_wrapper()

