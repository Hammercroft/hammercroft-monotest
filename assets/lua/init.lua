-- Initial Lua Script
print("Hello from Lua 5.1!")

-- Let's try to create an entity
local id = Engine.CreateEntity()
print("Created entity with ID: " .. id)

-- Let's play a sound
Engine.PlaySound("./assets/snd/boing.wav")

-- Sandbox verification
if os then
    print("Warning: 'os' library is available!")
else
    print("Sandbox Verified: 'os' library is NOT available.")
end

if dofile then
    print("Warning: 'dofile' is available!")
else
    print("Sandbox Verified: 'dofile' is NOT available.")
end

-- Time check
local t = Engine.GetTime()
print("Current Time: " .. t .. " seconds")

-- Set Background
Engine.SetBackgroundImage("./assets/bkg/testbackground.pbm")
