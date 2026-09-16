-- ============================================================
-- FPS Controller + LAN Multiplayer + R6 Animations + Hitboxes + MVP
-- Добавлены эффекты воды и пара по тегам <waterHole> и <steamHole>
-- Система закупки оружия (Buy Menu)
-- ============================================================

local BULLET_SPEED = 400.0 -- Скорость пули в м/с
--local BULLET_SEGMENT = 1.0 -- Длина сегмента для рейкаста (в метрах)

-- ГЕЙМПЛЕЙНЫЕ НАСТРОЙКИ СЕРВЕРА (переопределяются из UI хоста)
restoreOldWeaponCostMultipiler = 1.0
restoreDefaultWeaponsOnRoundStart = true
startMoney = 100000
maxMoney = 100000
restorMoneyOnRoundStart = false
roundBuyTime = 20.0
roundWonMoney = 0
roundLoseMoney = 0
killMoney = 0
deathMoney = 0
removeWeaponsOnDeath = true
roundsToWin = 9
tieRound = 8

ambSnd = nil

-- Элементы UI для настроек (заполняются в showConnectScreen)
cfgStartMoneyEl = nil
cfgMaxMoneyEl = nil
cfgBuyTimeEl = nil
cfgCostMultEl = nil
cfgRestoreDefEl = nil
cfgRestoreMonEl = nil
cfgMoneyWinEl = nil
cfgMoneyLoseEl = nil
cfgMoneyKillEl = nil
cfgMoneyDeathEl = nil
cfgRemoveWeaponsDeathEl = nil
cfgRoundsToWinEl = nil
cfgTieRoundEl = nil

gameMode = "competitive"
cfgGameModeEl = nil

centerImageActive = false
centerImageTimer = 0.0
centerImageBaseVh = 18.0
centerImageAmpVh = 1.4
centerImageSpeed = 10.0

Camera.setFOV(65)

setMaterialParam("VentedMetalWall", "flags", 95)
setMaterialParam("Asphalt", "flags", 95)
setMaterialParam("MetalGrill", "metallic", 0.95)
setMaterialParam("MetalGrill", "roughness", 0.0)

setMaterialParam("RustedMetal2", "metallic", 0.8)
setMaterialParam("RustedMetal2", "roughness", 0.2)

setMaterialParam("Water","customParam1",vec4(0.1, 5, 0.07, 0.3))
setMaterialParam("Water","customParam2",vec4(2, 10, 40, 0))
setMaterialParam("WaterDist","customParam1",vec4(0.1, 5.0, 0.07, 0.3))
setMaterialParam("WaterDist","customParam2",vec4(2, 10, 40, 3))

setMaterialParam("CeramicTiles","roughness",0.1)

--Graphics.fogDensity = 0.01
Graphics.ambientIntensity = 0.1
Graphics.useSSR = true
Graphics.ssrMaxSteps = 256
Graphics.ssrThickness = 0.5
Graphics.ssrFadeDistance = 50.0

Sound.setMasterVolume(0.5)

--Sound.setEnvironmentConfig(Sound.OpenSpace, 0.4, true, false, 5.0, 25.0, 0.9)
--Sound.setEnvironment(Sound.OpenSpace)

Sound.setEnvironmentConfig(Sound.Hangar, 0.85, true, false, 8.0, 30.0, 0.6)
Sound.setEnvironment(Sound.Hangar)
Sound.setEchoParameters(0.12, 0.25)

ColGroup = ColGroup or {}
ColGroup.NOTHING = 0
ColGroup.WORLD = 1
ColGroup.MOVEMENT = 2
ColGroup.HITBOX = 4
ColGroup.DYNAMIC = 8

playerName = "FPSPlayer"
myID = 0
myTeam = 0
myName = "Player"
mySkin = "navalniySkin1"
myMusicKit = "muskit1_mvp"

walkSpeed = 6.0
sprintSpeed = 12.0
airControl = 0.35
jumpForce = 7.0
mouseSens = 0.3
invertMouseX = true
invertMouseY = true
OBJ_SYNC_RATE = 0.1

damageBody = 16
damageHead = 50
damageArm = 8
damageLeg = 6

objSyncTimer = 0.0
R6_SCALE = 0.4
MAX_VELOCITY = 30.0
PLAYER_WIDTH = 0.8
PLAYER_HEIGHT = 1.8
PLAYER_HALF_W = PLAYER_WIDTH / 2
PLAYER_HALF_H = PLAYER_HEIGHT / 2
eyeOffset = 0.65
camFwdOffset = 0.2

playerMass = 80.0
shootRange = 100.0
fireRate = 0.08
maxAmmo = 30
currentAmmo = 30
isReloading = false
isPumping = false
pumpTimer = 0.0
isPumpReloading = false
pumpReloadTimer = 0.0
pumpReloadRemaining = 0
recoilKick = 1.5
recoilRecovery = 10.0
vmKickBack = 0.1
vmKickUp = 15.0
spreadAngle = 1.5
maxBulletHoles = 100
bulletsToFire = 1

footstepTimer = 0.0

function getStepDelay(speed)
    local s = math.max(3.0, math.min(24.0, speed))
    local t = (s - 3.0) / 21.0
    return 0.7 - (0.6 * t)
end

-- ==========================================
-- БАЗА ДАННЫХ ОРУЖИЯ
-- ==========================================
function defaultReloadSFX()
    Sound.play2D("pullBackStock1", 0.5, 1.0, false)
    wait(0.4)
    Sound.play2D("magOut1", 0.5, 1.0, false)
    wait(1.4)
    Sound.play2D("magIn1", 0.5, 1.0, false)
    wait(1.0)
    Sound.play2D("boltSlap1", 0.5, 1.0, false)
    wait(0.2)
end 

function minigunReloadSFX()
    Sound.play2D("minigunReload1", 0.5, 1.0, false)
    wait(1.0)
    Sound.play2D("minigunReload2", 0.5, 1.0, false)
    wait(2.0)
    Sound.play2D("minigunReload3", 0.5, 1.0, false)
    wait(1.0)
    Sound.play2D("minigunReload4", 0.5, 1.0, false)
    wait(1.0)
    Sound.play2D("minigunReload5", 0.5, 1.0, false)
end

function emptyReloadSFX()
    -- Ничего не делаем (для ножа)
end

function defaultReloadSFX3D(pos)
    Sound.play3D("pullBackStock1", pos, 2.0, 1.0, false)
    wait(0.4)
    Sound.play3D("magOut1", pos, 2.0, 1.0, false)
    wait(1.4)
    Sound.play3D("magIn1", pos, 2.0, 1.0, false)
    wait(1.0)
    Sound.play3D("boltSlap1", pos, 2.0, 1.0, false)
end

function minigunReloadSFX3D(pos)
    Sound.play3D("minigunReload1", pos, 2.0, 1.0, false)
    wait(1.0)
    Sound.play3D("minigunReload2", pos, 2.0, 1.0, false)
    wait(2.0)
    Sound.play3D("minigunReload3", pos, 2.0, 1.0, false)
    wait(1.0)
    Sound.play3D("minigunReload4", pos, 2.0, 1.0, false)
    wait(1.0)
    Sound.play3D("minigunReload5", pos, 2.0, 1.0, false)
    wait(1.0)
end

WEAPONS = {
    [1] = {
        name = "MP5K", slot = 1, price = 3000, shootRange = 1000, vmRight=0.3,vmFwd=0.6,vmUp=-0.25,
        fireRate = 0.06, spreadAngle = 1.5, recoilKick = 0.9*2.0, bulletsToFire = 1,
        damageBody = 16, damageHead = 50, damageArm = 8, damageLeg = 6,
        maxAmmo = 30, sound = "shot9", fireType = "automatic",
        burstDelay = 0.0, burstAmount = 1, sprintSpeed = 9.0, aimedFov = 55,
        walkSpeed = 6.0, reloadTime = 2.0, reloadSFX = defaultReloadSFX,
        reloadSFX3D = defaultReloadSFX3D, ammoType = "normal", ammoParameter = 0,
        viewmodelData = {
            parts = {
                { name = "Barrel", mesh = "MP5KPDW", mat = "MP5KPDW_0" },
                { name = "BoltCarrier", mesh = "MP5KPDW_1", mat = "MP5KPDW_0" },
                { name = "Chamber", mesh = "MP5KPDW_2", mat = "MP5KPDW_0" },
                { name = "ChargingHandle", mesh = "MP5KPDW_3", mat = "MP5KPDW_0" },
                { name = "ChargingHandleNub", mesh = "MP5KPDW_4", mat = "MP5KPDW_0" },
                { name = "Clip1", mesh = "MP5KPDW_5", mat = "MP5KPDW_0" },
                { name = "Clip2", mesh = "MP5KPDW_6", mat = "MP5KPDW_0" },
                { name = "ECLANSpecterAimOcclude", mesh = "MP5KPDW_7", mat = "MP5KPDW_1" },
                { name = "ECLANSpecterOptic", mesh = "MP5KPDW_10", mat = "MP5KPDW_1" },
                { name = "GasBlock", mesh = "MP5KPDW_11", mat = "MP5KPDW_0" },
                { name = "GasTube", mesh = "MP5KPDW_12", mat = "MP5KPDW_0" },
                { name = "Handguard", mesh = "MP5KPDW_13", mat = "MP5KPDW_0" },
                { name = "HandguardMount", mesh = "MP5KPDW_14", mat = "MP5KPDW_0" },
                { name = "LowerReceiver", mesh = "MP5KPDW_15", mat = "MP5KPDW_0" },
                { name = "Mag", mesh = "MP5KPDW_16", mat = "MP5KPDW_0" },
                { name = "MagFollower", mesh = "MP5KPDW_17", mat = "MP5KPDW_0" },
                { name = "MagRelease", mesh = "MP5KPDW_18", mat = "MP5KPDW_0" },
                { name = "Muzzle", mesh = "MP5KPDW_19", mat = "MP5KPDW_0" },
                { name = "Pad1", mesh = "MP5KPDW_20", mat = "MP5KPDW_0" },
                { name = "Pad2", mesh = "MP5KPDW_21", mat = "MP5KPDW_0" },
                { name = "Picatinny", mesh = "MP5KPDW_22", mat = "MP5KPDW_0" },
                { name = "Pin1", mesh = "MP5KPDW_23", mat = "MP5KPDW_0" },
                { name = "Pin2", mesh = "MP5KPDW_24", mat = "MP5KPDW_0" },
                { name = "RearSightAperture", mesh = "MP5KPDW_25", mat = "MP5KPDW_0" },
                { name = "RearSightBase", mesh = "MP5KPDW_26", mat = "MP5KPDW_0" },
                { name = "ReceiverCap", mesh = "MP5KPDW_27", mat = "MP5KPDW_0" },
                { name = "Selector", mesh = "MP5KPDW_28", mat = "MP5KPDW_0" },
                { name = "SlingSwivel", mesh = "MP5KPDW_29", mat = "MP5KPDW_0" },
                { name = "SlingSwivelBase", mesh = "MP5KPDW_30", mat = "MP5KPDW_0" },
                { name = "Stock", mesh = "MP5KPDW_31", mat = "MP5KPDW_0" },
                { name = "StockBase", mesh = "MP5KPDW_32", mat = "MP5KPDW_0" },
                { name = "StockHinge", mesh = "MP5KPDW_33", mat = "MP5KPDW_0" },
                { name = "Trigger", mesh = "MP5KPDW_34", mat = "MP5KPDW_0" },
                { name = "Tube", mesh = "MP5KPDW_35", mat = "MP5KPDW_0" },
                { name = "UpperReceiver", mesh = "MP5KPDW_36", mat = "MP5KPDW_0" },
                { name = "VLArm", mesh = "R6", mat = "r6mat_1" },
                { name = "VRArm", mesh = "R6", mat = "r6mat_1" },
            },
            animFile = "res/anims/mp5k_viewmodel.peaf",
            scale = vec3(0.3, 0.3, 0.3)
        }, doTracer = true,
		baseSpread = 0.3, spreadPerShot = 0.6, maxSpread = 4.5, recoilPerShot = 0.4, recoverySpeed = 5.5
    },
    [2] = {
        name = "USP45", slot = 2, price = 1000, shootRange = 1000,
        fireRate = 0.08, spreadAngle = 0.3, recoilKick = 0.7*2.0, bulletsToFire = 1,
        damageBody = 25, damageHead = 80, damageArm = 15, damageLeg = 10,
        maxAmmo = 12, sound = "shot7", fireType = "single",
        burstDelay = 0.0, burstAmount = 1, sprintSpeed = 9.2,
        walkSpeed = 6.2, reloadTime = 1.5, reloadSFX = defaultReloadSFX,
        reloadSFX3D = defaultReloadSFX3D, ammoType = "normal", ammoParameter = 0,
        viewmodelData = nil, doTracer = true,
		baseSpread = 0.1, spreadPerShot = 1.8, maxSpread = 6.0, recoilPerShot = 0.8, recoverySpeed = 6.0
    },
    [3] = {
        name = "G36K", slot = 1, price = 3200, shootRange = 1000, vmRight=-0.3,vmFwd=0.6,vmUp=-0.8,
        fireRate = 0.05, spreadAngle = 2.5, recoilKick = 1.0*2.0, bulletsToFire = 1,
        damageBody = 18, damageHead = 45, damageArm = 10, damageLeg = 8,
        maxAmmo = 36, sound = "shot4", fireType = "automatic",
        burstDelay = 0.06, burstAmount = 3, sprintSpeed = 8.8,
        walkSpeed = 5.8, reloadTime = 2.2, reloadSFX = defaultReloadSFX,
        reloadSFX3D = defaultReloadSFX3D, ammoType = "normal", ammoParameter = 0,
        viewmodelData = {
            parts = {
                { name = "Barrel", mesh = "G36K_4", mat = "G36K" },
				{ name = "Bolt", mesh = "G36K_2", mat = "G36K" },
				{ name = "ChargingHandle", mesh = "G36K_1", mat = "G36K" },
				{ name = "Handguard", mesh = "G36K", mat = "G36K" },
				{ name = "LowerReceiver", mesh = "G36K_3", mat = "G36K" },
				{ name = "Mag", mesh = "G36K_12", mat = "G36K" },
				{ name = "MagFollower", mesh = "G36K_11", mat = "G36K" },
				{ name = "MagRelease", mesh = "G36K_10", mat = "G36K" },
				{ name = "Optic", mesh = "G36K_9", mat = "G36K" },
				{ name = "Selector", mesh = "G36K_7", mat = "G36K" },
				{ name = "Sight", mesh = "G36K_8", mat = "G36K" },
				{ name = "Stock", mesh = "G36K_6", mat = "G36K" },
				{ name = "Trigger", mesh = "G36K_5", mat = "G36K" },
                { name = "VLArm", mesh = "R6", mat = "r6mat_1" },
                { name = "VRArm", mesh = "R6", mat = "r6mat_1" },
            },
            animFile = "res/anims/g36k_viewmodel.peaf",
            scale = vec3(0.3, 0.3, 0.3)
        }, doTracer = true,
		baseSpread = 0.2, spreadPerShot = 0.7, maxSpread = 5.5, recoilPerShot = 0.5, recoverySpeed = 4.0
    },
    [4] = {
        name = "MK18", slot = 1, price = 4600, shootRange = 1000, vmRight=-0.3,vmFwd=0.6,vmUp=-0.8,
        fireRate = 0.076, spreadAngle = 1.0, recoilKick = 1.1*2.0, bulletsToFire = 1,
        damageBody = 18, damageHead = 45, damageArm = 10, damageLeg = 8,
        maxAmmo = 30, sound = "shot3", fireType = "automatic",
        burstDelay = 0.0, burstAmount = 1, sprintSpeed = 8.8, aimedFov = 50,
        walkSpeed = 5.8, reloadTime = 2.1, reloadSFX = defaultReloadSFX,
        reloadSFX3D = defaultReloadSFX3D, ammoType = "normal", ammoParameter = 0,
		viewmodelData = {
            parts = {
                { name = "Barrel", mesh = "MK18_16", mat = "MK18_3" },
				{ name = "Bolt", mesh = "MK18_15", mat = "MK18_0" },
				{ name = "BoltCatch", mesh = "MK18_14", mat = "MK18_0" },
				{ name = "BufferTube", mesh = "MK18_13", mat = "MK18_5" },
				{ name = "ChargingHandle", mesh = "MK18_12", mat = "MK18_5" },
				{ name = "EOTechEXPS3", mesh = "MK18_11", mat = "MK18_4" },
				{ name = "Foregrip", mesh = "MK18_10", mat = "MK18_1" },
				{ name = "Grip", mesh = "MK18_9", mat = "MK18_1" },
				{ name = "Handguard1", mesh = "MK18_8", mat = "MK18_3" },
				{ name = "Handguard2", mesh = "MK18_7", mat = "MK18_3" },
				{ name = "Laser", mesh = "MK18_6", mat = "MK18_1" },
				{ name = "Mag", mesh = "MK18_4", mat = "MK18_2" },
				{ name = "MagRelease", mesh = "MK18_5", mat = "MK18_0" },
				{ name = "RearSight", mesh = "MK18_3", mat = "MK18_1" },
				{ name = "RearSightBase", mesh = "MK18_2", mat = "MK18_1" },
				{ name = "Receiver", mesh = "MK18_1", mat = "MK18_0" },
				{ name = "Selector", mesh = "MK18", mat = "MK18_0" },
				{ name = "Stock", mesh = "MK18_18", mat = "MK18_5" },
				{ name = "Trigger", mesh = "MK18_17", mat = "MK18_0" },
				{ name = "VLArm", mesh = "R6", mat = "r6mat_1" },
                { name = "VRArm", mesh = "R6", mat = "r6mat_1" },
            },
            animFile = "res/anims/mk18_viewmodel.peaf",
            scale = vec3(0.3, 0.3, 0.3)
        }, doTracer = true,
		baseSpread = 0.1, spreadPerShot = 0.5, maxSpread = 4.5, recoilPerShot = 0.6, recoverySpeed = 5.0
    },
    [5] = {
        name = "AK74", slot = 1, price = 4200, shootRange = 1000, vmRight=-0.3,vmFwd=0.6,vmUp=-0.8,
        fireRate = 0.1, spreadAngle = 2.8, recoilKick = 1.3*2.0, bulletsToFire = 1,
        damageBody = 24, damageHead = 55, damageArm = 12, damageLeg = 10,
        maxAmmo = 30, sound = "shot1", fireType = "automatic",
        burstDelay = 0.0, burstAmount = 1, sprintSpeed = 8.6,
        walkSpeed = 5.6, reloadTime = 2.4, reloadSFX = defaultReloadSFX,
        reloadSFX3D = defaultReloadSFX3D, ammoType = "normal", ammoParameter = 0,
		viewmodelData = {
            parts = {
                { name = "Barrel", mesh = "AK74_1", mat = "AK74_1" },
				{ name = "Bolt", mesh = "AK74", mat = "AK74_0" },
				{ name = "DustCover", mesh = "AK74_15", mat = "AK74_3" },
				{ name = "Grip", mesh = "AK74_14", mat = "AK74_7" },
				{ name = "Handguard", mesh = "AK74_13", mat = "AK74_6" },
				{ name = "Mag", mesh = "AK74_12", mat = "AK74_5" },
				{ name = "MagCatch", mesh = "AK74_11", mat = "AK74_0" },
				{ name = "MagFollower", mesh = "AK74_10", mat = "AK74_5" },
				{ name = "Muzzle", mesh = "AK74_9", mat = "AK74_4" },
				{ name = "RearSight", mesh = "AK74_8", mat = "AK74_0" },
				{ name = "RearSightAdjust", mesh = "AK74_7", mat = "AK74_0" },
				{ name = "Receiver", mesh = "AK74_6", mat = "AK74_0" },
				{ name = "ReceiverBits", mesh = "AK74_5", mat = "AK74_3" },
				{ name = "Selector", mesh = "AK74_4", mat = "AK74_0" },
				{ name = "Stock", mesh = "AK74_3", mat = "AK74_2" },
				{ name = "Trigger", mesh = "AK74_2", mat = "AK74_0" },
				{ name = "VLArm", mesh = "R6", mat = "r6mat_1" },
                { name = "VRArm", mesh = "R6", mat = "r6mat_1" },
            },
            animFile = "res/anims/ak74_viewmodel.peaf",
            scale = vec3(0.3, 0.3, 0.3)
        }, doTracer = true,
		baseSpread = 0.3, spreadPerShot = 1.0, maxSpread = 7.0, recoilPerShot = 0.7, recoverySpeed = 5.0
    },
    [6] = {
        name = "G17", slot = 2, price = 0, shootRange = 1000, vmRight=-0.3,vmFwd=0.6,vmUp=-0.8,
        fireRate = 0.08, spreadAngle = 0.5, recoilKick = 0.6*2.0, bulletsToFire = 1,
        damageBody = 20, damageHead = 60, damageArm = 12, damageLeg = 8,
        maxAmmo = 17, sound = "shot6", fireType = "single",
        burstDelay = 0.0, burstAmount = 1, sprintSpeed = 9.5, aimedFov = 60,
        walkSpeed = 6.5, reloadTime = 1.5, reloadSFX = defaultReloadSFX,
        reloadSFX3D = defaultReloadSFX3D, ammoType = "normal", ammoParameter = 0,
		viewmodelData = {
            parts = {
                { name = "Barrel", mesh = "G17_1", mat = "G17_0" },
				{ name = "Bolt", mesh = "G17", mat = "G17_0" },
				{ name = "Mag", mesh = "G17_2", mat = "G17_0" },
				{ name = "Reciever", mesh = "G17_3", mat = "G17_0" },
				{ name = "VLArm", mesh = "R6", mat = "r6mat_1" },
                { name = "VRArm", mesh = "R6", mat = "r6mat_1" },
            },
            animFile = "res/anims/g17_viewmodel.peaf",
            scale = vec3(0.3, 0.3, 0.3)
        }, doTracer = true,
		baseSpread = 0.15, spreadPerShot = 1.4, maxSpread = 5.0, recoilPerShot = 0.6, recoverySpeed = 6.5
    },
    [7] = {
        name = "M134", slot = 1, price = 17000, shootRange = 1000,
        fireRate = 0.05, spreadAngle = 3.5, recoilKick = 0.5*2.0, bulletsToFire = 1,
        damageBody = 10, damageHead = 20, damageArm = 6, damageLeg = 3,
        maxAmmo = 100, sound = "minigunShootLoop1", fireType = "minigun",
        burstDelay = 0.0, burstAmount = 1, sprintSpeed = 6.0,
        chargeTime = 0.6, chargeSound = "minigunWindStart1",
        stopSound = "minigunWindStop1", fireSoundType = "minigun",
        walkSpeed = 3.0, reloadTime = 6.0, reloadSFX = minigunReloadSFX,
        reloadSFX3D = minigunReloadSFX3D, ammoType = "normal", ammoParameter = 0,
        viewmodelData = nil, doTracer = true,
		baseSpread = 1.5, spreadPerShot = 0.05, maxSpread = 3.0, recoilPerShot = 0.0, recoverySpeed = 2.0
    },
    [8] = {
        name = "BarrettMK22", slot = 1, price = 10000, shootRange = 1000,
        fireRate = 1.5, spreadAngle = 0.0, recoilKick = 3.0*2.0, bulletsToFire = 1,
        damageBody = 100, damageHead = 200, damageArm = 50, damageLeg = 50,
        maxAmmo = 5, sound = "shot8", fireType = "single",
        burstDelay = 0.0, burstAmount = 1,
        walkSpeed = 4.0, sprintSpeed = 8.0, reloadTime = 3.5, aimedFov = 10,
        reloadSFX = defaultReloadSFX, reloadSFX3D = defaultReloadSFX3D,
        ammoType = "penetrative", ammoParameter = 3,
        viewmodelData = nil, doTracer = true,
		baseSpread = 0.0, spreadPerShot = 0.0, maxSpread = 0.0, recoilPerShot = 0.0, recoverySpeed = 10.0
    },
    [9] = {
        name = "RPG7", slot = 1, price = 16000, shootRange = 1000, vmRight=-0.1,vmFwd=0.6,vmUp=-0.8,
        fireRate = 1.0, spreadAngle = 0.0, recoilKick = 3.0*2.0, bulletsToFire = 1,
        damageBody = 50, damageHead = 50, damageArm = 50, damageLeg = 50,
        maxAmmo = 1, sound = "rocketLaunch1", fireType = "single",
        burstDelay = 0.0, burstAmount = 1,
        walkSpeed = 4.0, sprintSpeed = 7.0, reloadTime = 4.0,
        reloadSFX = defaultReloadSFX, reloadSFX3D = defaultReloadSFX3D,
        isProjectile = true, projectileType = "explosive",
        projectileSpeed = 150.0, projectileTimer = 2.0,
        explodeOnImpact = true, explosionForce = 150,
		viewmodelData = {
            parts = {
                { name = "FrontSight", mesh = "RPG7_5", mat = "RPG7_2" },
				{ name = "Hammer", mesh = "RPG7_2", mat = "RPG7_1" },
				{ name = "RearSight", mesh = "RPG7_4", mat = "RPG7_2" },
				{ name = "Receiver", mesh = "RPG7_6", mat = "RPG7_1" },
				{ name = "Rocket", mesh = "RPG7", mat = "RPG7_0" },
				{ name = "SightsBase", mesh = "RPG7_7", mat = "RPG7_2" },
				{ name = "Stock", mesh = "RPG7_3", mat = "RPG7_1" },
				{ name = "Trigger", mesh = "RPG7_1", mat = "RPG7_1" },
				{ name = "VLArm", mesh = "R6", mat = "r6mat_1" },
                { name = "VRArm", mesh = "R6", mat = "r6mat_1" },
            },
            animFile = "res/anims/rpg7_viewmodel.peaf",
            scale = vec3(0.3, 0.3, 0.3)
        }, doTracer = false,
        baseSpread = 0.0, spreadPerShot = 0.0, maxSpread = 0.0, recoilPerShot = 0.0, recoverySpeed = 10.0
    },
    [10] = {
        name = "M67", slot = 4, price = 300,
        fireRate = 2.0, spreadAngle = 0.0, recoilKick = 0.0, bulletsToFire = 1,
        maxAmmo = 1, sound = "grenadePin1", fireType = "single",
        walkSpeed = 6.0, sprintSpeed = 12.0, reloadTime = 0.0,
        reloadSFX = emptyReloadSFX, reloadSFX3D = emptyReloadSFX,
        isProjectile = true, projectileType = "explosive",
        projectileSpeed = 15.0, projectileTimer = 3.0,
        explodeOnImpact = false, explosionForce = 150,
        viewmodelData = nil, doTracer = true
    },
    [11] = {
        name = "M18", slot = 4, price = 200,
        fireRate = 2.0, spreadAngle = 0.0, recoilKick = 0.0, bulletsToFire = 1,
        maxAmmo = 1, sound = "grenadePin1", fireType = "single",
        walkSpeed = 6.0, sprintSpeed = 12.0, reloadTime = 0.0,
        reloadSFX = emptyReloadSFX, reloadSFX3D = emptyReloadSFX,
        isProjectile = true, projectileType = "smoke",
        projectileSpeed = 15.0, projectileTimer = 3.0,
        explodeOnImpact = false, explosionForce = 0,
        viewmodelData = nil, doTracer = true
    },
    [12] = {
        name = "M84", slot = 4, price = 200,
        fireRate = 2.0, spreadAngle = 0.0, recoilKick = 0.0, bulletsToFire = 1,
        maxAmmo = 1, sound = "grenadePin1", fireType = "single",
        walkSpeed = 6.0, sprintSpeed = 12.0, reloadTime = 0.0,
        reloadSFX = emptyReloadSFX, reloadSFX3D = emptyReloadSFX,
        isProjectile = true, projectileType = "flashbang",
        projectileSpeed = 15.0, projectileTimer = 1.5,
        explodeOnImpact = false, explosionForce = 0,
        viewmodelData = nil, doTracer = true
    },
    [13] = {
        name = "Knife", slot = 3, price = 0,
        fireRate = 0.4, spreadAngle = 0.0, recoilKick = 0.0, bulletsToFire = 1,
        damageBody = 55, damageHead = 150, damageArm = 40, damageLeg = 40,
        maxAmmo = 9999, sound = "knife1", fireType = "automatic",
        burstDelay = 0.0, burstAmount = 1,
        walkSpeed = 7.0, sprintSpeed = 10.0, reloadTime = 0.0,
        reloadSFX = emptyReloadSFX, reloadSFX3D = emptyReloadSFX,
        shootRange = 1.5,
        viewmodelData = nil, doTracer = false
    },
    [14] = {
        name = "M249", slot = 1, price = 5200, shootRange = 1000,
        fireRate = 0.08, spreadAngle = 2.3, recoilKick = 1.3*2.0, bulletsToFire = 1,
        damageBody = 20, damageHead = 45, damageArm = 10, damageLeg = 8,
        maxAmmo = 200, sound = "shot2", fireType = "automatic",
        burstDelay = 0.0, burstAmount = 1, sprintSpeed = 6.0,
        walkSpeed = 4.0, reloadTime = 5.0, reloadSFX = defaultReloadSFX, reloadSFX3D = defaultReloadSFX3D,
        ammoType = "normal", ammoParameter = 0,
        viewmodelData = nil, doTracer = true,
		baseSpread = 0.3, spreadPerShot = 0.3, maxSpread = 6.5, recoilPerShot = 0.3, recoverySpeed = 3.5
    },
	[15] = {
		name = "SHORT870", slot = 1, price = 2400, shootRange = 1000,
		fireRate = 1.2, spreadAngle = 10.0, recoilKick = 3.0*2.0, bulletsToFire = 12,
		damageBody = 10, damageHead = 15, damageArm = 4, damageLeg = 2,
		maxAmmo = 7, sound = "shot10", fireType = "shotgun", pumpSound = "shotgunPump1",
		burstDelay = 0.0, burstAmount = 1, sprintSpeed = 9.0,
		walkSpeed = 6.0, reloadTime = 5.0, reloadSFX = defaultReloadSFX, reloadSFX3D = defaultReloadSFX3D,
		ammoType = "normal", ammoParameter = 0,
		viewmodelData = nil, doTracer = true,
		baseSpread = 10.0, spreadPerShot = 0.0, maxSpread = 10.0, recoilPerShot = 0.0, recoverySpeed = 10.0,
		reloadType = "pump",
		pumpReloadInterval = 0.6
	},
	[16] = {
        name = "WARP", slot = 4, price = 600,
        fireRate = 0.15, spreadAngle = 0.0, recoilKick = 0.0, bulletsToFire = 1,
        maxAmmo = 100, sound = "grenadePin1", fireType = "automatic",
        walkSpeed = 6.0, sprintSpeed = 12.0, reloadTime = 0.0,
        reloadSFX = emptyReloadSFX, reloadSFX3D = emptyReloadSFX,
        isProjectile = true, projectileType = "warpGrenade",
        projectileSpeed = 15.0, projectileTimer = 3.0,
        explodeOnImpact = false, explosionForce = 150,
        viewmodelData = nil, doTracer = true
    },
	[17] = {
        name = "BOMBER", slot = 4, price = 20000,
        fireRate = 0.2, spreadAngle = 0.0, recoilKick = 0.0, bulletsToFire = 1,
        maxAmmo = 100, sound = "grenadePin1", fireType = "automatic",
        walkSpeed = 6.0, sprintSpeed = 12.0, reloadTime = 0.0,
        reloadSFX = emptyReloadSFX, reloadSFX3D = emptyReloadSFX,
        isProjectile = true, projectileType = "explosive",
        projectileSpeed = 20.0, projectileTimer = 3.0,
        explodeOnImpact = false, explosionForce = 150,
        viewmodelData = nil, doTracer = true
    },
    [18] = {
        name = "RPGAUTO", slot = 1, price = 40000, shootRange = 1000, vmRight=-0.1,vmFwd=0.6,vmUp=-0.8,
        fireRate = 0.06, spreadAngle = 7.0, recoilKick = 1.5, bulletsToFire = 1,
        damageBody = 50, damageHead = 50, damageArm = 50, damageLeg = 50,
        maxAmmo = 400, sound = "shot9", fireType = "automatic",
        burstDelay = 0.0, burstAmount = 1,
        walkSpeed = 4.0, sprintSpeed = 7.0, reloadTime = 4.0,
        reloadSFX = defaultReloadSFX, reloadSFX3D = defaultReloadSFX3D,
        isProjectile = true, projectileType = "explosive",
        projectileSpeed = 70.0, projectileTimer = 5.0,
        explodeOnImpact = true, explosionForce = 300,
		viewmodelData = {
            parts = {
                { name = "FrontSight", mesh = "RPG7_5", mat = "RPG7_2" },
				{ name = "Hammer", mesh = "RPG7_2", mat = "RPG7_1" },
				{ name = "RearSight", mesh = "RPG7_4", mat = "RPG7_2" },
				{ name = "Receiver", mesh = "RPG7_6", mat = "RPG7_1" },
				{ name = "Rocket", mesh = "RPG7", mat = "RPG7_0" },
				{ name = "SightsBase", mesh = "RPG7_7", mat = "RPG7_2" },
				{ name = "Stock", mesh = "RPG7_3", mat = "RPG7_1" },
				{ name = "Trigger", mesh = "RPG7_1", mat = "RPG7_1" },
				{ name = "VLArm", mesh = "R6", mat = "r6mat_1" },
                { name = "VRArm", mesh = "R6", mat = "r6mat_1" },
            },
            animFile = "res/anims/rpg7_viewmodel.peaf",
            scale = vec3(0.3, 0.3, 0.3)
        }, doTracer = false,
        baseSpread = 7.0, spreadPerShot = 0.0, maxSpread = 0.0, recoilPerShot = 0.0, recoverySpeed = 10.0
    },
    [19] = {
        name = "Shovel", slot = 3, price = 0,
        fireRate = 0.4, spreadAngle = 0.0, recoilKick = 0.0, bulletsToFire = 1,
        damageBody = 55, damageHead = 150, damageArm = 40, damageLeg = 40,
        maxAmmo = 9999, sound = "knife1", fireType = "automatic",
        burstDelay = 0.0, burstAmount = 1,
        walkSpeed = 7.0, sprintSpeed = 10.0, reloadTime = 0.0,
        reloadSFX = emptyReloadSFX, reloadSFX3D = emptyReloadSFX,
        shootRange = 1.5,
        viewmodelData = nil, doTracer = false,
        hitSound = "shovelHit1"
    },
}

weaponsSlots = { [1] = nil, [2] = 6, [3] = 13, [4] = nil }
actualSlotsAmmo = { [1] = nil, [2] = nil, [3] = nil, [4] = nil }
money = startMoney
currentWeapon = 6

-- Buy Menu States
isBuyMenuOpen = false
buyMenuDoc = nil
moneyTextEl = nil
bWasDown = false
roundTime = 0.0
hasReceivedCFG = false

burstShotsRemaining = 0
burstCooldownTimer = 0.0
canShoot = true
lmbWasDown = false

-- Minigun states
isCharging = false
chargeTimer = 0.0
isMinigunLooping = false
minigunLoopSoundId = 0

camYaw = 0.0
camPitch = 0.0
isGrounded = false
wantJump = false
shootCooldown = 0.0
rWasDown = false

-- ==========================================
-- СОСТОЯНИЕ ПРИЦЕЛИВАНИЯ (ADS)
-- ==========================================
local isAiming = false
local currentAimFov = 65.0
local targetAimFov = 65.0
local defaultFov = 65.0

vmRecoilPos = 0.0
vmRecoilRot = 0.0
-- Динамическая отдача и спред
currentSpreadMult = 0.0
currentRecoilMult = 0.0
bobPhase = 0.0
bobAmountX = 0.0
bobAmountY = 0.0
vmTiltZ = 0.0
vmJumpZ = 0.0
vmMouseSwayX = 0.0
vmMouseSwayY = 0.0

camVisYaw = 0.0
camVisPitch = 0.0
camIdleTimer = 0.0
vmMouseRoll = 0.0
vmNeedsIdleReturn = false

camRecoilQueue = 0.0
camRecoilRecovery = 0.0

local function file_exists(path)
    local file = io.open(path, "r")
    if file then
        file:close()
        return true
    end
    return false
end

-- ==========================================
-- КАРТА СЕРВЕРА
-- ==========================================
currentMap = "backrooms"

-- Теперь значение карты — это путь к ресурсному файлу.
-- Если строка пустая "", то Resources.load вызываться не будет.
MAP_LOOKUP = {
	["backrooms"] = "",
	["poolrooms"] = "",
	["sigma_arena"] = "",
	["kal1"] = "",
	["kal2"] = "",
}

function loadMap(map, postfix)
	postfix = postfix or ""
    if type(map) ~= "string" then return end
    local resourceFile = "res/maps/"..map.."/res"..postfix..".json"
	local mapFile = "res/maps/"..map.."/map"..postfix..".pemf"

    if not file_exists(mapFile) then end

    if Resources and Resources.load and file_exists(resourceFile) then
        pcall(function()
			Resources.unloadAll()
			Resources.load("resources.json")
            Resources.load(resourceFile)
            setMaterialParam("VentedMetalWall", "flags", 95)
            setMaterialParam("Asphalt", "flags", 95)
            setMaterialParam("MetalGrill", "metallic", 0.95)
            setMaterialParam("MetalGrill", "roughness", 0.0)

            setMaterialParam("RustedMetal2", "metallic", 0.8)
            setMaterialParam("RustedMetal2", "roughness", 0.2)

            setMaterialParam("Water","customParam1",vec4(0.1, 5, 0.07, 0.3))
            setMaterialParam("Water","customParam2",vec4(2, 10, 40, 0))
            setMaterialParam("WaterDist","customParam1",vec4(0.1, 5.0, 0.07, 0.3))
            setMaterialParam("WaterDist","customParam2",vec4(2, 10, 40, 3))

            setMaterialParam("CeramicTiles","roughness",0.1)
        end)
    end

	Engine.loadScene(mapFile)
end

cfgMapEl = nil

hasReceivedInit = false
pendingStartAfterInit = false
hostLastPacketTime = 0.0

-- Сколько секунд клиент ждёт пакеты от хоста перед выходом
HOST_TIMEOUT = 40.0

-- Сколько раз в секунду отправляем POS
-- 0.033 = примерно 30 пакетов в секунду
-- Если всё равно лагает -- ставь 0.05 (20 пакетов в секунду)
NET_POS_RATE = 0.033
lastNetPosTime = 0.0

-- Heartbeat от хоста
hostBeatTimer = 0.0

-- Хост проверяет, не отвалились ли клиенты
hostCheckTimer = 0.0

-- Клиент шлёт PING хосту
pingSendTimer = 0.0

-- Сколько секунд хост ждёт активности от клиента
CLIENT_TIMEOUT = 40.0

-- Активные клиенты у хоста: id -> время последнего пакета
activeClients = {}

-- Заблокированные ID после дисконнекта, чтобы мертвые игроки не восставали
blockedIDs = {}

-- ==========================================
-- HOST SESSION / CLIENT IDS / RESPAWN TOKENS
-- ==========================================
nextClientID = 2
tokenToID = {}

clientToken = 0
hasReceivedID = false

helloTimer = 0.0
pingTimer = 0.0

respawnTokens = {}

gameStarted = false
isPaused = false
pauseDoc = nil
escWasDown = false
preGameInitTimer = 0.0

netMode = "none"
maxHealth = 200
myHealth = maxHealth
isDead = false
roundCooldown = false
spectateTargetID = 0

caCurrent = 0.005
caTarget = 0.005
caStartValue = 0.005
caFadeTimer = 0.0
caFadeDuration = 0.8
caIncrement = 0.015
caBaseAtFullHealth = 0.005
caBaseAtZeroHealth = 0.015

-- Flash effect variables (brightness & blur)
flashActive = false
flashOrigBrightness = 0.5
flashOrigBlur = 0.0
flashTargetBrightness = 2.0
flashTargetBlur = 5.0
flashFadeTimer = 0.0
flashFadeDuration = 3.0

remotePlayers = {}
pendingNames = {}
pendingSkins = {}
dynamicBoxes = {}
deadBodyParts = {}
spawnPointsRed = {}
spawnPointsBlue = {}

myChar = nil
viewmodelAnim = nil
viewmodelPartIDs = {}
bulletHoles = {}
bulletHoleCounter = 0

teamScores = { [1] = 0, [2] = 0 }
roundActive = true
killsThisRound = {}
firstKillerID = 0
mvpName = ""
mvpReason = ""
mvpID = 0

hudDoc = nil
healthTextEl = nil
healthBarEl = nil
ammoTextEl = nil
reloadTextEl = nil
scoreRedEl = nil
scoreBlueEl = nil
roundResultEl = nil
mvpContainerEl = nil
mvpTextEl = nil
mvpTitleEl = nil
deathScreenEl = nil
killerNameEl = nil
centerImageEl = nil

killNotifEl = nil
killNotifActive = false
killNotifPhase = "idle" -- "idle", "stay", "fade"
killNotifTimer = 0.0
killNotifBaseMarginVh = 1.4
killNotifFadeMoveVh = 2.0

connectDoc = nil
teamSelectDoc = nil
ipInputEl = nil
nameInputEl = nil
skinBtns = {}
musicBtns = {}

-- ==========================================
-- ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
-- ==========================================
function setTeamColor(obj, team)
    if team == 1 then
        obj.paintColor = vec4(1.0, 0.2, 0.2, 1.0)
    elseif team == 2 then
        obj.paintColor = vec4(0.2, 0.4, 1.0, 1.0)
    else
        obj.paintColor = vec4(1.0, 1.0, 1.0, 1.0)
    end
end

function applySkinToChar(char, skin)
    if not char then return end
    char.body.material = skin
    char.head.material = skin
    char.rArm.material = skin
    char.lArm.material = skin
    char.rLeg.material = skin
    char.lLeg.material = skin
    -- Внешние части (если они есть)
    if char.HeadOut then char.HeadOut.material = skin end
    if char.BodyOut then char.BodyOut.material = skin end
    if char.lArmOut then char.lArmOut.material = skin end
    if char.lLegOut then char.lLegOut.material = skin end
    if char.rArmOut then char.rArmOut.material = skin end
    if char.rLegOut then char.rLegOut.material = skin end
end

function safePhysicsCall(func, ...)
    local success, err = pcall(func, ...)
    if not success then
        -- Игнорируем ошибки физики
    end
end

particleTimers = {}
FXCounter = 0

function scheduleParticleRemoval(obj, duration)
    if not obj then return end
    local deadTime = Scene.getTime() + duration
    particleTimers[obj.name] = deadTime
end

function cleanupExpiredParticles()
    local now = Scene.getTime()
    for objName, deadTime in pairs(particleTimers) do
        if now >= deadTime then
            local obj = Object.find(objName)
            if obj then obj:destroy() end
            particleTimers[objName] = nil
        end
    end
end

function createWaterParticles(point, normal)
    FXCounter = FXCounter + 1
    local ps = Object.new("particleemitter", "WaterFX_" .. FXCounter)
    ps.position = point + normal * 0.05
    ps.texture = "water_spray"
    ps.flipbookGrid = vec2(4, 4)
    ps.flipbookFPS = 24
    ps.randomStartFrame = true
    ps.rate = 0
    ps.direction = normal
    ps.lifetime = vec2(0.8, 1.5)
    ps.gravity = vec3(0, 0, -5.0)
    ps.spreadAngle = vec2(15, 70)
    ps.speed = vec2(2.0, 5.0)
    ps.brightness = vec2(0.6, 1.0)
    ps.opacity = vec2(2.0, 3.0)
    ps.color = Gradient.new({
        {0.0, vec4(0.3, 0.7, 1.0, 0.8)},
        {0.7, vec4(0.2, 0.5, 0.9, 0.5)},
        {1.0, vec4(0.1, 0.3, 0.6, 0.0)}
    })
    ps.size = NumSequence.new(0.08, 0.4)
    ps:emit(40)
    scheduleParticleRemoval(ps, 20.0)
end

function createSteamParticles(point, normal)
    FXCounter = FXCounter + 1
    local ps = Object.new("particleemitter", "SteamFX_" .. FXCounter)
    ps.position = point + normal * 0.1
    ps.texture = "smoke2"
    ps.flipbookGrid = vec2(8, 8)
    ps.flipbookFPS = 15
    ps.randomStartFrame = true
    ps.rate = 0
    ps.direction = normal + vec3(0, 0, 1)
    ps.lifetime = vec2(1.2, 2.5)
    ps.gravity = vec3(0, 0, 1.5)
    ps.spreadAngle = vec2(10, 50)
    ps.speed = vec2(0.5, 2.0)
    ps.brightness = vec2(0.5, 0.8)
    ps.opacity = vec2(0.6, 1.0)
    ps.color = Gradient.new(
        vec4(0.9, 0.9, 0.9, 0.7),
        vec4(0.8, 0.8, 0.8, 0.0)
    )
    ps.size = NumSequence.new(0.2, 0.8)
    ps:emit(25)
    scheduleParticleRemoval(ps, 20.0)
end

function dist(a, b)
    local dx = a.x - b.x
    local dy = a.y - b.y
    local dz = a.z - b.z
    return math.sqrt(dx * dx + dy * dy + dz * dz)
end

function clearScene()
    for _, obj in ipairs(Object.list()) do
        obj:destroy()
    end
    remotePlayers = {}
    pendingNames = {}
    pendingSkins = {}
    myChar = nil
    bulletHoles = {}
    bulletHoleCounter = 0
    dynamicBoxes = {}
    spawnPointsRed = {}
    spawnPointsBlue = {}
    Sound.stopAll()
end

-- ==========================================
-- INIT / CFG / PRE-GAME NETWORKING
-- ==========================================
function applyCfgPacket(parts)
    if netMode ~= "client" or #parts ~= 14 then
        return
    end

    startMoney = tonumber(parts[2]) or startMoney
    maxMoney = tonumber(parts[3]) or maxMoney
    roundBuyTime = tonumber(parts[4]) or roundBuyTime

    restoreDefaultWeaponsOnRoundStart = tonumber(parts[5]) == 1
    restorMoneyOnRoundStart = tonumber(parts[6]) == 1
    restoreOldWeaponCostMultipiler = tonumber(parts[7]) or 1.0

    roundWonMoney = tonumber(parts[8]) or 0
    roundLoseMoney = tonumber(parts[9]) or 0
    killMoney = tonumber(parts[10]) or 0
    deathMoney = tonumber(parts[11]) or 0

    removeWeaponsOnDeath = tonumber(parts[12]) == 1
    roundsToWin = tonumber(parts[13]) or 9
    tieRound = tonumber(parts[14]) or 8

    if not hasReceivedCFG then
        money = startMoney
        hasReceivedCFG = true
    end

    if money > maxMoney then
        money = maxMoney
    end

    if money < 0 then
        money = 0
    end

    if isBuyMenuOpen then
        updateBuyMenu()
    end

    if hudDoc then
        updateHUD()
    end
end

function sendServerInit()
    if netMode ~= "host" then
        return
    end

    Network.send(string.format("INIT %s", currentMap))
    Network.send(string.format("MODE %s", gameMode))

    local rDef = restoreDefaultWeaponsOnRoundStart and 1 or 0
    local rMon = restorMoneyOnRoundStart and 1 or 0
    local rWep = removeWeaponsOnDeath and 1 or 0

    Network.send(string.format(
        "CFG %d %d %f %d %d %f %d %d %d %d %d %d %d",
        startMoney, maxMoney, roundBuyTime,
        rDef, rMon, restoreOldWeaponCostMultipiler,
        roundWonMoney, roundLoseMoney, killMoney, deathMoney,
        rWep, roundsToWin, tieRound
    ))
end

function processPreGameNetworking()
    local packets = Network.receive() or {}

    if netMode == "client" and #packets > 0 then
        hostLastPacketTime = Scene.getTime()
    end

    local shouldStartGame = false

    for _, packet in ipairs(packets) do
        local parts = {}
        for word in packet:gmatch("%S+") do
            table.insert(parts, word)
        end

        if parts[1] == "HB" then
            -- Heartbeat от хоста.
            -- Ничего не делаем, hostLastPacketTime уже обновился выше.

        elseif parts[1] == "PING" and #parts == 2 then
            if netMode == "host" then
                hostTouchClient(tonumber(parts[2]), true)
            end

        elseif parts[1] == "END" then
            if netMode == "client" then
                showHostDisconnected()
                return
            end

        elseif parts[1] == "ASSIGN" and #parts >= 3 then
            if netMode == "client" then
                local id = tonumber(parts[2])
                local token = tonumber(parts[3])

                if id and token == clientToken then
                    myID = id
                    hasReceivedID = true

                    if pendingStartAfterInit and hasReceivedInit and myTeam ~= 0 then
                        shouldStartGame = true
                    end
                end
            end

        elseif parts[1] == "INIT" and #parts == 2 then
            if netMode == "client" then
                if MAP_LOOKUP[parts[2]] and not gameStarted then
                    currentMap = parts[2]
                end

                hasReceivedInit = true

                if pendingStartAfterInit and hasReceivedID and myTeam ~= 0 and myID ~= 0 then
                    shouldStartGame = true
                end
            end

        elseif parts[1] == "HELLO" then
            if netMode == "host" then
                handleHello(parts)
            end

        elseif parts[1] == "CFG" and #parts == 14 then
            applyCfgPacket(parts)

        elseif parts[1] == "MODE" and #parts == 2 then
            gameMode = parts[2]

        elseif parts[1] == "NAME" and #parts >= 3 then
            local id = tonumber(parts[2])
            if id then
                pendingNames[id] = table.concat(parts, " ", 3, #parts)
            end

        elseif parts[1] == "SKIN" and #parts >= 3 then
            local id = tonumber(parts[2])
            if id then
                pendingSkins[id] = table.concat(parts, " ", 3, #parts)
            end
        end
    end

    if shouldStartGame then
        pendingStartAfterInit = false

        if teamSelectDoc then
            RmlUi.destroy(teamSelectDoc)
            teamSelectDoc = nil
        end

        startGame()
    end
end

-- ==========================================
-- NETWORK HELPERS
-- ==========================================
function isPlayerActive(id)
    if id == myID then
        return true
    end

    if netMode == "host" then
        return activeClients[id] ~= nil
    end

    return true
end

function invalidateRespawn(id)
    respawnTokens[id] = (respawnTokens[id] or 0) + 1
end
--[[
function removeRemotePlayer(id)
    if id == myID then
        return
    end

    local rp = remotePlayers[id]
    if rp then
        if rp.minigunLoopSoundId then
            Sound.stop(rp.minigunLoopSoundId)
        end

        if rp.physRoot and rp.physRoot.name then
            safePhysicsCall(Physics.destroyBody, rp.physRoot.name)
            pcall(rp.physRoot.destroy, rp.physRoot)
        end

        if rp.body then pcall(rp.body.destroy, rp.body) end
        if rp.head then pcall(rp.head.destroy, rp.head) end
        if rp.rArm then pcall(rp.rArm.destroy, rp.rArm) end
        if rp.lArm then pcall(rp.lArm.destroy, rp.lArm) end
        if rp.rLeg then pcall(rp.rLeg.destroy, rp.rLeg) end
        if rp.lLeg then pcall(rp.lLeg.destroy, rp.lLeg) end

        if rp.HeadOut then pcall(rp.HeadOut.destroy, rp.HeadOut) end
        if rp.BodyOut then pcall(rp.BodyOut.destroy, rp.BodyOut) end
        if rp.lArmOut then pcall(rp.lArmOut.destroy, rp.lArmOut) end
        if rp.lLegOut then pcall(rp.lLegOut.destroy, rp.lLegOut) end
        if rp.rArmOut then pcall(rp.rArmOut.destroy, rp.rArmOut) end
        if rp.rLegOut then pcall(rp.rLegOut.destroy, rp.rLegOut) end

        if rp.anim then pcall(rp.anim.destroy, rp.anim) end
    end

    remotePlayers[id] = nil
    pendingNames[id] = nil
    pendingSkins[id] = nil

    if spectateTargetID == id then
        spectateTargetID = 0
    end
end
]]--
function handleHello(parts)
    if netMode ~= "host" then return end
    local token = tonumber(parts[2])
    if not token then return end
    local id = tokenToID[token]
    if not id then
        id = nextClientID
        nextClientID = nextClientID + 1
        tokenToID[token] = id
    end
    activeClients[id] = Scene.getTime()
    Network.send(string.format("ASSIGN %d %d", id, token))
    sendServerInit()
end

-- ==========================================
-- NETWORK KEEPALIVE / DISCONNECT HELPERS
-- ==========================================
function hostTouchClient(id, allowUnblock)
    if not id or id == myID then
        return
    end

    activeClients[id] = Scene.getTime()

    if allowUnblock then
        blockedIDs[id] = nil
    end
end

function removeRemotePlayer(id)
    if not id then
        return
    end

    local rp = remotePlayers[id]

    if rp then
        if rp.minigunLoopSoundId then
            Sound.stop(rp.minigunLoopSoundId)
        end

        -- Сначала убиваем анимацию, чтобы она не держала привязки
        if rp.anim then
            pcall(rp.anim.destroy, rp.anim)
        end

        local refs = {
            rp.physRoot,
            rp.body,
            rp.head,
            rp.rArm,
            rp.lArm,
            rp.rLeg,
            rp.lLeg,
            rp.HeadOut,
            rp.BodyOut,
            rp.lArmOut,
            rp.lLegOut,
            rp.rArmOut,
            rp.rLegOut
        }

        for _, ref in ipairs(refs) do
            if ref and ref.name then
                safePhysicsCall(Physics.destroyBody, ref.name)
                pcall(ref.destroy, ref)
            end
        end
    end

    remotePlayers[id] = nil
    pendingNames[id] = nil
    pendingSkins[id] = nil

    if spectateTargetID == id then
        spectateTargetID = 0
    end

    -- Удаляем гранаты/снаряды, которые принадлежали этому игроку
    local prefix = "Grenade_" .. tostring(id) .. "_"
    for _, obj in ipairs(Object.list()) do
        if obj and obj.name and string.sub(obj.name, 1, #prefix) == prefix then
            safePhysicsCall(Physics.destroyBody, obj.name)
            pcall(obj.destroy, obj)
        end
    end
end

function hostRemoveClient(id, broadcast)
    if not id then return end
    invalidateRespawn(id)          -- ← добавить эту строку
    blockedIDs[id] = true
    activeClients[id] = nil
    removeRemotePlayer(id)
    if broadcast then
        Network.send(string.format("DISCONNECT %d", id))
    end
end

-- ==========================================
-- PAUSE MENU
-- ==========================================
function openPauseMenu()
    if isPaused or not gameStarted or isDead then
        return
    end

    local doc = RmlUi.loadDocument("scripts/pause_menu.rml")
    if not doc then
        return
    end

    if isBuyMenuOpen then
        closeBuyMenu()
    end

    pauseDoc = doc
    isPaused = true

	isAiming = false
    targetAimFov = defaultFov

    RmlUi.show(pauseDoc, 2, 1)
	

    local resumeBtn = RmlUi.getElementById(pauseDoc, "btn-resume")
    local disconnectBtn = RmlUi.getElementById(pauseDoc, "btn-disconnect")

    if resumeBtn then
        RmlUi.addEventListener(resumeBtn, "click", function()
            closePauseMenu()
        end)
    end

	if disconnectBtn then
		RmlUi.addEventListener(disconnectBtn, "click", function()
			if netMode == "host" then
				hostLeave()
			else
				returnToMainMenu()
			end
		end)
	end

    if viewmodelAnim then
        viewmodelAnim.scale = vec3(0, 0, 0)
    end

    Camera.setInputMode(InputMode.None)
end

function closePauseMenu()
    if not isPaused then
        if pauseDoc then
            RmlUi.destroy(pauseDoc)
            pauseDoc = nil
        end
        return
    end

    isPaused = false

    if pauseDoc then
        RmlUi.destroy(pauseDoc)
        pauseDoc = nil
    end

    if viewmodelAnim and not isDead then
        viewmodelAnim.scale = vec3(0.3, 0.3, 0.3)
    end

    if gameStarted then
        Camera.setInputMode(InputMode.Game2)
    end
end

function processPausedMovement(dt)
    if not myChar then
        return
    end

    local vel = Physics.getVelocity(myChar.physRoot.name)

    if isGrounded then
        Physics.setVelocity(myChar.physRoot.name, vec3(0, 0, vel.z))
    end

    local ppos = Physics.getPosition(myChar.physRoot.name)

    local yawRad = math.rad(camYaw)
    local hFwdX, hFwdY = math.cos(yawRad), math.sin(yawRad)

    local eyePos = ppos + vec3(0, 0, eyeOffset) + vec3(hFwdX, hFwdY, 0) * camFwdOffset

    local pitchRad = math.rad(camPitch)
    local lookDir = vec3(
        math.cos(yawRad) * math.cos(pitchRad),
        math.sin(yawRad) * math.cos(pitchRad),
        math.sin(pitchRad)
    )

    Camera.setPosition(eyePos)
    Camera.lookAt(eyePos + lookDir)

    updateLocalAnimation()
end

function showHostDisconnected()
	if netMode == "none" then
		return
	end

    if pauseDoc then
        RmlUi.destroy(pauseDoc)
        pauseDoc = nil
    end

    if buyMenuDoc then
        RmlUi.destroy(buyMenuDoc)
        buyMenuDoc = nil
    end

    if hudDoc then
        RmlUi.destroy(hudDoc)
        hudDoc = nil
    end

    if teamSelectDoc then
        RmlUi.destroy(teamSelectDoc)
        teamSelectDoc = nil
    end

    if connectDoc then
        RmlUi.destroy(connectDoc)
        connectDoc = nil
    end

    if Network.isConnected() then
        Network.disconnect()
    end

    clearViewmodels()
    clearDeadBodyParts()
    clearScene()

    netMode = "none"
    gameStarted = false
    isPaused = false
    isBuyMenuOpen = false

    hasReceivedInit = false
    pendingStartAfterInit = false
    hasReceivedCFG = false
    preGameInitTimer = 0.0
	
	activeClients = {}
    blockedIDs = {}
    lastNetPosTime = 0.0
    hostBeatTimer = 0.0
    hostCheckTimer = 0.0
    pingSendTimer = 0.0
	
	nextClientID = 2
	tokenToID = {}
	respawnTokens = {}

	clientToken = 0
	hasReceivedID = false
	helloTimer = 0.0
	pingTimer = 0.0
	hostCheckTimer = 0.0

    myID = 0
    myTeam = 0
    myHealth = maxHealth
    isDead = false

    roundActive = true
    roundCooldown = false
    roundTime = 0.0

    spectateTargetID = 0
    teamScores = { [1] = 0, [2] = 0 }
    killsThisRound = {}
    firstKillerID = 0

    mvpID = 0
    mvpName = ""
    mvpReason = ""

    money = startMoney
    weaponsSlots = { [1] = nil, [2] = 6, [3] = 13, [4] = nil }
    actualSlotsAmmo = { [1] = nil, [2] = nil, [3] = nil, [4] = nil }

    currentWeapon = 6
    if WEAPONS[6] then
        maxAmmo = WEAPONS[6].maxAmmo
        currentAmmo = maxAmmo
    end

    centerImageActive = false
    killNotifActive = false
    killNotifPhase = "idle"
    killNotifTimer = 0.0
    flashActive = false

    Sound.stopAll()

    Graphics.saturation = 1.3
    Graphics.brightness = 0.5
    Graphics.blurAmount = 0.0

    Camera.setFOV(65)
    Camera.setInputMode(InputMode.None)

    showConnectScreen()

    if connectDoc then
        local title = RmlUi.getElementById(connectDoc, "connect-title")
        if title then
            RmlUi.setInnerRML(title, "Удалённый хост разорвал соединение")
        end
    end
end

function hostLeave()
    if netMode == "host" then
        Network.send("END")
    end

    returnToMainMenu()
end

function returnToMainMenu()
    if netMode == "host" and Network.isConnected() then
        Network.send("END")
    end

    if netMode == "client" and Network.isConnected() and myID ~= 0 then
        Network.send(string.format("DISCONNECT %d", myID))
    end

    if Network.isConnected() then
        Network.disconnect()
    end

    if pauseDoc then
        RmlUi.destroy(pauseDoc)
        pauseDoc = nil
    end

    if buyMenuDoc then
        RmlUi.destroy(buyMenuDoc)
        buyMenuDoc = nil
    end

    if hudDoc then
        RmlUi.destroy(hudDoc)
        hudDoc = nil
    end

    if teamSelectDoc then
        RmlUi.destroy(teamSelectDoc)
        teamSelectDoc = nil
    end

    if connectDoc then
        RmlUi.destroy(connectDoc)
        connectDoc = nil
    end

    healthTextEl = nil
    healthBarEl = nil
    ammoTextEl = nil
    reloadTextEl = nil
    scoreRedEl = nil
    scoreBlueEl = nil
    roundResultEl = nil
    mvpContainerEl = nil
    mvpTextEl = nil
    mvpTitleEl = nil
    deathScreenEl = nil
    killerNameEl = nil
    centerImageEl = nil
    killNotifEl = nil
    moneyTextEl = nil

    clearViewmodels()
    clearDeadBodyParts()
    clearScene()

    netMode = "none"
    gameStarted = false
    isPaused = false
    isBuyMenuOpen = false

    hasReceivedInit = false
    pendingStartAfterInit = false
    hasReceivedCFG = false
    preGameInitTimer = 0.0
	
	activeClients = {}
    blockedIDs = {}
    lastNetPosTime = 0.0
    hostBeatTimer = 0.0
    hostCheckTimer = 0.0
    pingSendTimer = 0.0

	nextClientID = 2
	tokenToID = {}
	respawnTokens = {}

	clientToken = 0
	hasReceivedID = false
	helloTimer = 0.0
	pingTimer = 0.0
	hostCheckTimer = 0.0

    myID = 0
    myTeam = 0
    myHealth = maxHealth
    isDead = false

    roundActive = true
    roundCooldown = false
    roundTime = 0.0

    spectateTargetID = 0
    teamScores = { [1] = 0, [2] = 0 }
    killsThisRound = {}
    firstKillerID = 0

    mvpID = 0
    mvpName = ""
    mvpReason = ""

    money = startMoney
    weaponsSlots = { [1] = nil, [2] = 6, [3] = 13, [4] = nil }
    actualSlotsAmmo = { [1] = nil, [2] = nil, [3] = nil, [4] = nil }

    currentWeapon = 6
    if WEAPONS[6] then
        maxAmmo = WEAPONS[6].maxAmmo
        currentAmmo = maxAmmo
    end

    centerImageActive = false
    killNotifActive = false
    killNotifPhase = "idle"
    killNotifTimer = 0.0
    flashActive = false

    Sound.stopAll()

    Graphics.saturation = 1.3
    Graphics.brightness = 0.5
    Graphics.blurAmount = 0.0

    Camera.setFOV(65)
    Camera.setInputMode(InputMode.None)

    showConnectScreen()
end

-- ==========================================
-- СОЗДАНИЕ ПЕРСОНАЖЕЙ
-- ==========================================
-- Уничтожает текущую вьюмодель
function clearViewmodels()
    -- 🔥 ЖИРНАЯ ОПТИМИЗАЦИЯ: Уничтожаем детали по сохранённым ID.
    -- Никакого Object.list() и сравнения строк! Фриз при переключении исчезнет навсегда.
    if viewmodelPartIDs then
        for _, objId in pairs(viewmodelPartIDs) do
            if objId then
                pcall(Object.destroy, objId)
            end
        end
    end
    viewmodelPartIDs = {}

    -- Уничтожаем анимационный контроллер
    if viewmodelAnim then
        pcall(viewmodelAnim.destroy, viewmodelAnim)
        viewmodelAnim = nil
    end
end

-- Создаёт вьюмодель для указанного оружия по его viewmodelData
function createViewmodelsForWeapon(weaponId)
    clearViewmodels()
    local w = WEAPONS[weaponId]
    if not w or not w.viewmodelData then
        return  -- нет данных – ничего не создаём
    end

    local data = w.viewmodelData
    viewmodelPartIDs = {}

    -- Создаём детали
    for i, partData in ipairs(data.parts) do
        local idx = i - 1  -- треки анимации нумеруются с 0
        local objName = "Viewmodel_" .. partData.name
        local obj = Object.new("mesh", objName)
        obj.mesh = partData.mesh
        obj.material = partData.mat
        obj.position = vec3(0, 0, 0)
        obj.rotation = vec3(0, 0, 0)
        obj.scale = vec3(1, 1, 1)
        viewmodelPartIDs[idx] = obj.id
    end

    -- Создаём анимационный контроллер
    viewmodelAnim = Object.new("animation", "Viewmodel_animation")
    viewmodelAnim:loadClip(data.animFile)
    viewmodelAnim:clearBindings()

    -- Привязываем детали к трекам
    for i, partData in ipairs(data.parts) do
        local idx = i - 1
        local id = viewmodelPartIDs[idx]
        if id then
            viewmodelAnim:attachObject(id, idx, partData.name)
        end
    end

    -- Устанавливаем масштаб
    local scale = data.scale or vec3(0.3, 0.3, 0.3)
    viewmodelAnim.scale = scale
    viewmodelAnim:playClip("idle")
end

function spawnLocalR6(startPos)
	local physRoot = Object.new("body", playerName)
	physRoot.mesh = ""
	physRoot.material = ""

	-- ВАЖНО: ставим объект в спавн ДО создания физики
	physRoot.position = startPos

	Physics.createBody(
		physRoot.name,
		playerMass,
		vec3(PLAYER_HALF_W, PLAYER_HALF_W, PLAYER_HALF_H),
		"box", nil, nil,
		ColGroup.MOVEMENT,
		ColGroup.WORLD + ColGroup.DYNAMIC
	)

	Physics.setAngularFactor(physRoot.name, vec3(0, 0, 0))
	Physics.setCcd(physRoot.name, 0.5, 0.2)

	-- На всякий случай ещё раз принудительно ресетим тело в спавн
	Physics.reset(physRoot.name, startPos)
	Physics.setVelocity(physRoot.name, vec3(0, 0, 0))
	Physics.setAngularVelocity(physRoot.name, vec3(0, 0, 0))
	Physics.activate(physRoot.name)

    local body = Object.new("body", "R6_Body_Local")
    Physics.createBody(
        body.name, 0,
        vec3(0.112 * R6_SCALE * 2, 0.202 * R6_SCALE * 2, 0.291 * R6_SCALE * 2),
        "box", nil, nil,
        ColGroup.HITBOX, ColGroup.NOTHING
    )
    Physics.setCollisionEnabled(body.name, false)
    Physics.setKinematic(body.name, true)
    body.mesh = "Steve"
    body.material = "navalniySkin1"

    local head = Object.new("body", "R6_Head_Local")
    Physics.createBody(
        head.name, 0,
        vec3(0.202 * R6_SCALE * 2, 0.202 * R6_SCALE * 2, 0.202 * R6_SCALE * 2),
        "box", nil, nil,
        ColGroup.HITBOX, ColGroup.NOTHING
    )
    Physics.setCollisionEnabled(head.name, false)
    Physics.setKinematic(head.name, true)
    head.mesh = ""
    head.material = ""
	
	local brr = Object.new("light", "flash")
	brr.parent = head
	brr.attachToParent = true
	brr.castShadow = true
	brr.position = vec3(1,0,0)
	brr.color = vec3(1.0, 1.0, 0.7)
	brr.intensity = 0
	brr.radius = 50
	brr.enabled = true

    local rArm = Object.new("body", "R6_RArm_Local")
    Physics.createBody(
        rArm.name, 0,
        vec3(0.112 * R6_SCALE * 2, 0.112 * R6_SCALE * 2, 0.291 * R6_SCALE * 2),
        "box", nil, nil,
        ColGroup.HITBOX, ColGroup.NOTHING
    )
    Physics.setCollisionEnabled(rArm.name, false)
    Physics.setKinematic(rArm.name, true)
    rArm.mesh = ""
    rArm.material = ""

    local lArm = Object.new("body", "R6_LArm_Local")
    Physics.createBody(
        lArm.name, 0,
        vec3(0.112 * R6_SCALE * 2, 0.112 * R6_SCALE * 2, 0.291 * R6_SCALE * 2),
        "box", nil, nil,
        ColGroup.HITBOX, ColGroup.NOTHING
    )
    Physics.setCollisionEnabled(lArm.name, false)
    Physics.setKinematic(lArm.name, true)
    lArm.mesh = ""
    lArm.material = ""

    local rLeg = Object.new("body", "R6_RLeg_Local")
    Physics.createBody(
        rLeg.name, 0,
        vec3(0.112 * R6_SCALE * 2, 0.112 * R6_SCALE * 2, 0.291 * R6_SCALE * 2),
        "box", nil, nil,
        ColGroup.HITBOX, ColGroup.NOTHING
    )
    Physics.setCollisionEnabled(rLeg.name, false)
    Physics.setKinematic(rLeg.name, true)
    rLeg.mesh = "Steve_10"
    rLeg.material = "navalniySkin1"

    local lLeg = Object.new("body", "R6_LLeg_Local")
    Physics.createBody(
        lLeg.name, 0,
        vec3(0.112 * R6_SCALE * 2, 0.112 * R6_SCALE * 2, 0.291 * R6_SCALE * 2),
        "box", nil, nil,
        ColGroup.HITBOX, ColGroup.NOTHING
    )
    Physics.setCollisionEnabled(lLeg.name, false)
    Physics.setKinematic(lLeg.name, true)
    lLeg.mesh = "Steve_6"
    lLeg.material = "navalniySkin1"

    -- ===== НОВЫЕ ДЕТАЛИ =====
	local bodyOut = Object.new("mesh", "R6_BodyOut_Local")
    bodyOut.mesh = "Steve_1"
    bodyOut.material = "navalniySkin1"
	
	local headOut = Object.new("mesh", "R6_HeadOut_Local")
    headOut.mesh = ""
    headOut.material = ""

    local lArmOut = Object.new("mesh", "R6_LArmOut_Local")
    lArmOut.mesh = ""
    lArmOut.material = ""

    local lLegOut = Object.new("mesh", "R6_LLegOut_Local")
    lLegOut.mesh = "Steve_7"
    lLegOut.material = "navalniySkin1"

    local rArmOut = Object.new("mesh", "R6_RArmOut_Local")
    rArmOut.mesh = ""
    rArmOut.material = ""

    local rLegOut = Object.new("mesh", "R6_RLegOut_Local")
    rLegOut.mesh = "Steve_11"
    rLegOut.material = "navalniySkin1"

    -- Анимационный контроллер
    local anim = Object.new("animation", "R6_Anim_Local")
    anim:loadClip("res/anims/mc_anim.peaf")
    anim:clearBindings()
    anim:attachObject(body.id, 1, "Body")
	anim:attachObject(bodyOut.id, 2, "BodyOut")
    anim:attachObject(head.id, 3, "Head")
    anim:attachObject(headOut.id, 4, "HeadOut")
	anim:attachObject(rArm.id, 9, "RArm")
    anim:attachObject(lArm.id, 5, "LArm")
    anim:attachObject(rLeg.id, 11, "RLeg")
    anim:attachObject(lLeg.id, 7, "LLeg")
    anim:attachObject(lArmOut.id, 6, "LArmOut")
    anim:attachObject(lLegOut.id, 8, "LLegOut")
    anim:attachObject(rArmOut.id, 10, "RArmOut")
    anim:attachObject(rLegOut.id, 12, "RLegOut")
    anim:playClip("idle")

    return {
        physRoot = physRoot,
        body = body,
        head = head,
        rArm = rArm,
        lArm = lArm,
        rLeg = rLeg,
        lLeg = lLeg,
		HeadOut = headOut,
		BodyOut = bodyOut,
        lArmOut = lArmOut,
        lLegOut = lLegOut,
        rArmOut = rArmOut,
        rLegOut = rLegOut,
		flash = brr,
        anim = anim,
        currentMoveState = "idle"
    }
end

function spawnRemoteR6(id, startPos)
    local physRoot = Object.new("body", "R6_Root_" .. id)
    physRoot.mesh = ""
    physRoot.material = ""
    Physics.createBody(
        physRoot.name, 0,
        vec3(PLAYER_HALF_W, PLAYER_HALF_W, PLAYER_HALF_H),
        "box", nil, nil,
        ColGroup.MOVEMENT,
        ColGroup.WORLD + ColGroup.DYNAMIC
    )
    Physics.setKinematic(physRoot.name, true)
    Physics.setAngularFactor(physRoot.name, vec3(0, 0, 0))

    local body = Object.new("body", "R6_Body_" .. id)
    Physics.createBody(
        body.name, 0,
        vec3(0.112 * R6_SCALE * 2, 0.202 * R6_SCALE * 2, 0.291 * R6_SCALE * 2),
        "box", nil, nil,
        ColGroup.HITBOX, ColGroup.NOTHING
    )
    Physics.setCollisionEnabled(body.name, false)
    Physics.setKinematic(body.name, true)
    body.mesh = "Steve"
    body.material = "navalniySkin1"

    local head = Object.new("body", "R6_Head_" .. id)
    Physics.createBody(
        head.name, 0,
        vec3(0.202 * R6_SCALE * 2, 0.202 * R6_SCALE * 2, 0.202 * R6_SCALE * 2),
        "box", nil, nil,
        ColGroup.HITBOX, ColGroup.NOTHING
    )
    Physics.setCollisionEnabled(head.name, false)
    Physics.setKinematic(head.name, true)
    head.mesh = "Steve_2"
    head.material = "navalniySkin1"

    local rArm = Object.new("body", "R6_RArm_" .. id)
    Physics.createBody(
        rArm.name, 0,
        vec3(0.112 * R6_SCALE * 2, 0.112 * R6_SCALE * 2, 0.291 * R6_SCALE * 2),
        "box", nil, nil,
        ColGroup.HITBOX, ColGroup.NOTHING
    )
    Physics.setCollisionEnabled(rArm.name, false)
    Physics.setKinematic(rArm.name, true)
    rArm.mesh = "Steve_8"
    rArm.material = "navalniySkin1"

    local lArm = Object.new("body", "R6_LArm_" .. id)
    Physics.createBody(
        lArm.name, 0,
        vec3(0.112 * R6_SCALE * 2, 0.112 * R6_SCALE * 2, 0.291 * R6_SCALE * 2),
        "box", nil, nil,
        ColGroup.HITBOX, ColGroup.NOTHING
    )
    Physics.setCollisionEnabled(lArm.name, false)
    Physics.setKinematic(lArm.name, true)
    lArm.mesh = "Steve_4"
    lArm.material = "navalniySkin1"

    local rLeg = Object.new("body", "R6_RLeg_" .. id)
    Physics.createBody(
        rLeg.name, 0,
        vec3(0.112 * R6_SCALE * 2, 0.112 * R6_SCALE * 2, 0.291 * R6_SCALE * 2),
        "box", nil, nil,
        ColGroup.HITBOX, ColGroup.NOTHING
    )
    Physics.setCollisionEnabled(rLeg.name, false)
    Physics.setKinematic(rLeg.name, true)
    rLeg.mesh = "Steve_10"
    rLeg.material = "navalniySkin1"

    local lLeg = Object.new("body", "R6_LLeg_" .. id)
    Physics.createBody(
        lLeg.name, 0,
        vec3(0.112 * R6_SCALE * 2, 0.112 * R6_SCALE * 2, 0.291 * R6_SCALE * 2),
        "box", nil, nil,
        ColGroup.HITBOX, ColGroup.NOTHING
    )
    Physics.setCollisionEnabled(lLeg.name, false)
    Physics.setKinematic(lLeg.name, true)
    lLeg.mesh = "Steve_6"
    lLeg.material = "navalniySkin1"
	
	local bodyOut = Object.new("mesh", "R6_BodyOut_" .. id)
    bodyOut.mesh = "Steve_1"
    bodyOut.material = "navalniySkin1"
	
	local headOut = Object.new("mesh", "R6_HeadOut_" .. id)
    headOut.mesh = "Steve_3"
    headOut.material = "navalniySkin1"
	
    local lArmOut = Object.new("mesh", "R6_LArmOut_" .. id)
    lArmOut.mesh = "Steve_5"
    lArmOut.material = "navalniySkin1"

    local lLegOut = Object.new("mesh", "R6_LLegOut_" .. id)
    lLegOut.mesh = "Steve_7"
    lLegOut.material = "navalniySkin1"

    local rArmOut = Object.new("mesh", "R6_RArmOut_" .. id)
    rArmOut.mesh = "Steve_9"
    rArmOut.material = "navalniySkin1"

    local rLegOut = Object.new("mesh", "R6_RLegOut_" .. id)
    rLegOut.mesh = "Steve_11"
    rLegOut.material = "navalniySkin1"

    local anim = Object.new("animation", "R6_Anim_" .. id)
    anim:loadClip("res/anims/mc_anim.peaf")
    anim:clearBindings()
    anim:attachObject(body.id, 1, "Body")
	anim:attachObject(bodyOut.id, 2, "BodyOut")
    anim:attachObject(head.id, 3, "Head")
	anim:attachObject(headOut.id, 4, "HeadOut")
    anim:attachObject(rArm.id, 9, "RArm")
    anim:attachObject(lArm.id, 5, "LArm")
    anim:attachObject(rLeg.id, 11, "RLeg")
    anim:attachObject(lLeg.id, 7, "LLeg")
	anim:attachObject(lArmOut.id, 6, "LArmOut")
	anim:attachObject(lLegOut.id, 8, "LLegOut")
	anim:attachObject(rArmOut.id, 10, "RArmOut")
	anim:attachObject(rLegOut.id, 12, "RLegOut")
    anim:playClip("idle")

    return {
        physRoot = physRoot,
        body = body,
        head = head,
        rArm = rArm,
        lArm = lArm,
        rLeg = rLeg,
        lLeg = lLeg,
		HeadOut = headOut,
		BodyOut = bodyOut,
        lArmOut = lArmOut,
        lLegOut = lLegOut,
        rArmOut = rArmOut,
        rLegOut = rLegOut,
        anim = anim,
        team = 0,
        isDead = false,
        currentMoveState = "idle",
        name = "Player_" .. id,
        skin = "navalniySkin1",
        musicKit = "muskit1_mvp",
        currentWeapon = 6,
        minigunLoopSoundId = nil,
        lastFootstepTime = 0.0
    }
end

function spawnDeadBodyParts(position, rotation, skinMaterial, team)
    local partMass = 5.0
    local group = ColGroup.DYNAMIC
    local mask = ColGroup.WORLD + ColGroup.DYNAMIC + ColGroup.MOVEMENT
    local parts = {
        {name = "LArm", mesh = "R6", material = "r6mat_1",
         size = vec3(1.0 * R6_SCALE, 1.0 * R6_SCALE, 2.0 * R6_SCALE),
         offset = vec3(-0.4, 0.2, 0.0), rotOffset = vec3(0, 0, 0)},
        {name = "RArm", mesh = "R6", material = "r6mat_1",
         size = vec3(1.0 * R6_SCALE, 1.0 * R6_SCALE, 2.0 * R6_SCALE),
         offset = vec3(0.4, 0.2, 0.0), rotOffset = vec3(0, 0, 0)},
        {name = "LLeg", mesh = "R6", material = "r6mat_1",
         size = vec3(1.0 * R6_SCALE, 1.0 * R6_SCALE, 2.0 * R6_SCALE),
         offset = vec3(-0.3, 0.0, -0.6), rotOffset = vec3(0, 0, 0)},
        {name = "RLeg", mesh = "R6", material = "r6mat_1",
         size = vec3(1.0 * R6_SCALE, 1.0 * R6_SCALE, 2.0 * R6_SCALE),
         offset = vec3(0.3, 0.0, -0.6), rotOffset = vec3(0, 0, 0)},
        {name = "Body", mesh = "R6", material = skinMaterial or "r6mat_0",
         size = vec3(2.0 * R6_SCALE, 1.0 * R6_SCALE, 2.0 * R6_SCALE),
         offset = vec3(0.0, 0.0, 0.0), rotOffset = vec3(0, 0, 0)},
        {name = "Head", mesh = "R6_1", material = "r6mat_1",
         size = vec3(1.0 * R6_SCALE, 1.0 * R6_SCALE, 1.0 * R6_SCALE),
         offset = vec3(0.0, 0.0, 0.8), rotOffset = vec3(0, 0, 0)}
    }
	
	local createdParts = {}
    for _, p in ipairs(parts) do
        local objName = "derbis_Player" .. p.name .. "_" .. FXCounter
        FXCounter = FXCounter + 1
        local obj = Object.new("body", objName)
        obj.mesh = p.mesh
        obj.material = p.material
        obj.scale = p.size
        obj.position = position + p.offset
        obj.rotation = rotation + p.rotOffset
        obj.castShadow = true
        setTeamColor(obj, team)
        Physics.createBody(obj.name, partMass, p.size / 2.0, "box", nil, nil, group, mask)
        Physics.setVelocity(obj.name, vec3(math.randf(-3, 3), math.randf(-3, 3), math.randf(2, 6)))
        Physics.setAngularVelocity(obj.name, vec3(math.randf(-5, 5), math.randf(-5, 5), math.randf(-5, 5)))
        table.insert(deadBodyParts, obj)
		table.insert(createdParts, obj)
    end

	if gameMode == "deathmatch" then
		-- Удаление трупа через 20 секунд
		local partsToRemove = createdParts
		
		spawn(function()
			wait(20.0)
			for _, obj in ipairs(partsToRemove) do
				if obj and obj.name then
					safePhysicsCall(Physics.destroyBody, obj.name)
					pcall(obj.destroy, obj)
					-- Чистим из глобального массива
					for i = #deadBodyParts, 1, -1 do
						if deadBodyParts[i] == obj then
							table.remove(deadBodyParts, i)
							break
						end
					end
				end
			end
		end, "removeBodies_"..tostring(FXCounter))
	end
end

function clearDeadBodyParts()
    for i, obj in ipairs(deadBodyParts) do
        if obj and obj.name then
            safePhysicsCall(Physics.destroyBody, obj.name)
            pcall(obj.destroy, obj)
        end
    end
    deadBodyParts = {}
end

-- ==========================================
-- HUD
-- ==========================================
function showKillNotification(text)
    if not killNotifEl then return end

    RmlUi.setInnerRML(killNotifEl, text)
    RmlUi.setProperty(killNotifEl, "opacity", "1")
    RmlUi.setProperty(killNotifEl, "margin-top", string.format("%.2fvh", killNotifBaseMarginVh))

    killNotifActive = true
    killNotifPhase = "stay"
    killNotifTimer = 0.0
end

function onLocalKill(victimID, isHeadshot)
    local sound8sec = { "panteri8sec", "shalava8sec", "tikitiki8sec", "ulicaroz8sec" }
    local centerImages = { "../res/textures/ui/trollface1.tga", "../res/textures/ui/trollface2.tga", "../res/textures/ui/blues1.tga", "../res/textures/ui/blues2.tga", "../res/textures/ui/blues3.tga", "../res/textures/ui/blues4.tga" }
    
    Sound.play2D(sound8sec[math.random(1, #sound8sec)], 1.0, 1.0, false)
    if isHeadshot then
        Sound.play2D("headshot", 1.0, 1.0, false)
    end

    if killMoney > 0 then
        showKillNotification("+ " .. killMoney .. " Aura")
    end
    
    showCenterImage(centerImages[math.random(1, #centerImages)])
    spawn(function()
        wait(8.0)
        hideCenterImage()
    end, "hideKillImage")
end

function updateHUD()
    if healthTextEl then
        RmlUi.setInnerRML(healthTextEl, "HP: " .. (isDead and 0 or myHealth))
    end
    if healthBarEl then
        RmlUi.setProperty(healthBarEl, "width", math.max(0, isDead and 0 or myHealth) .. "%")
    end
    if ammoTextEl then
        RmlUi.setInnerRML(ammoTextEl, currentAmmo .. " / " .. maxAmmo)
    end
    if reloadTextEl then
        if isReloading and not isDead then
            RmlUi.setProperty(reloadTextEl, "display", "block")
        else
            RmlUi.setProperty(reloadTextEl, "display", "none")
        end
    end
    if scoreRedEl then
        RmlUi.setInnerRML(scoreRedEl, tostring(teamScores[1]))
    end
    if scoreBlueEl then
        RmlUi.setInnerRML(scoreBlueEl, tostring(teamScores[2]))
    end
end

-- Показать картинку по пути
function showCenterImage(path)
    if centerImageEl then
        RmlUi.setAttribute(centerImageEl, "src", path)
        RmlUi.setProperty(centerImageEl, "display", "block")
        centerImageActive = true
        centerImageTimer = 0.0
    end
	Graphics.saturation = 0.2
end

-- Скрыть картинку
function hideCenterImage()
    if centerImageEl then
        RmlUi.setProperty(centerImageEl, "display", "none")
        centerImageActive = false
        Camera.setFOV(65)
        Graphics.saturation = 1.3

        RmlUi.setProperty(centerImageEl, "width", centerImageBaseVh .. "vh")
        RmlUi.setProperty(centerImageEl, "height", centerImageBaseVh .. "vh")
        RmlUi.setProperty(centerImageEl, "margin-left", string.format("%.2fvh", -centerImageBaseVh / 2.0))
    end
end

-- ==========================================
-- СОЕДИНЕНИЕ И НАСТРОЙКИ
-- ==========================================
function hostGame()
    if nameInputEl then
        myName = RmlUi.getAttribute(nameInputEl, "value")
        if myName == "" or string.len(myName) > 32 then
            myName = "Player"
        end
    end

    if cfgGameModeEl then
        gameMode = RmlUi.getAttribute(cfgGameModeEl, "value")
    end

    if cfgMapEl then
        local mapVal = RmlUi.getAttribute(cfgMapEl, "value")
        if MAP_LOOKUP[mapVal] then
            currentMap = mapVal
        end
    end

    if cfgStartMoneyEl then
        startMoney = tonumber(RmlUi.getAttribute(cfgStartMoneyEl, "value")) or 100000
    end

    if cfgMaxMoneyEl then
        maxMoney = tonumber(RmlUi.getAttribute(cfgMaxMoneyEl, "value")) or 100000
    end

    if cfgBuyTimeEl then
        roundBuyTime = tonumber(RmlUi.getAttribute(cfgBuyTimeEl, "value")) or 20.0
    end

    if cfgCostMultEl then
        restoreOldWeaponCostMultipiler = tonumber(RmlUi.getAttribute(cfgCostMultEl, "value")) or 1.0
    end

    if cfgRestoreDefEl then
        local rDefVal = tostring(RmlUi.getAttribute(cfgRestoreDefEl, "checked"))
        restoreDefaultWeaponsOnRoundStart = (rDefVal == "true" or rDefVal == "1")
    end

    if cfgRestoreMonEl then
        local rMonVal = tostring(RmlUi.getAttribute(cfgRestoreMonEl, "checked"))
        restorMoneyOnRoundStart = (rMonVal == "true" or rMonVal == "1")
    end

    if cfgRemoveWeaponsDeathEl then
        local rWepVal = tostring(RmlUi.getAttribute(cfgRemoveWeaponsDeathEl, "checked"))
        removeWeaponsOnDeath = (rWepVal == "true" or rWepVal == "1")
    end

    if cfgMoneyWinEl then
        roundWonMoney = tonumber(RmlUi.getAttribute(cfgMoneyWinEl, "value")) or 0
    end

    if cfgMoneyLoseEl then
        roundLoseMoney = tonumber(RmlUi.getAttribute(cfgMoneyLoseEl, "value")) or 0
    end

    if cfgMoneyKillEl then
        killMoney = tonumber(RmlUi.getAttribute(cfgMoneyKillEl, "value")) or 0
    end

    if cfgMoneyDeathEl then
        deathMoney = tonumber(RmlUi.getAttribute(cfgMoneyDeathEl, "value")) or 0
    end

    if cfgRoundsToWinEl then
        roundsToWin = tonumber(RmlUi.getAttribute(cfgRoundsToWinEl, "value")) or 9
    end

    if cfgTieRoundEl then
        tieRound = tonumber(RmlUi.getAttribute(cfgTieRoundEl, "value")) or 8
    end

    money = startMoney

    netMode = "host"
    myID = 1

    -- Новая сессия хоста
    nextClientID = 2
    activeClients = {}
    tokenToID = {}
    respawnTokens = {}

    hasReceivedInit = true
    hasReceivedID = true
    pendingStartAfterInit = false
    preGameInitTimer = 0.0

    Network.host(7777)
    sendServerInit()

    RmlUi.destroy(connectDoc)
    connectDoc = nil

    showTeamSelect()
end

function joinGame()
    local ip = "127.0.0.1"

    if ipInputEl then
        ip = RmlUi.getAttribute(ipInputEl, "value")
        if ip == "" then
            ip = "127.0.0.1"
        end
    end

    if nameInputEl then
        myName = RmlUi.getAttribute(nameInputEl, "value")
        if myName == "" or string.len(myName) > 32 then
            myName = "Player"
        end
    end

    netMode = "client"

    -- Клиент больше не придумывает себе окончательный ID.
    -- ID выдаст хост.
    myID = 0

    -- Токен нужен, чтобы хост мог узнать клиента при переподключении.
    clientToken = math.floor(Scene.getTime() * 1000) + math.random(0, 999999)

    hasReceivedID = false
    hasReceivedInit = false
    pendingStartAfterInit = false

    preGameInitTimer = 0.0
    helloTimer = 0.0
    pingTimer = 0.0
    hostLastPacketTime = Scene.getTime()

    Network.connect(ip, 7777)
    Network.send(string.format("HELLO %d", clientToken))

    RmlUi.destroy(connectDoc)
    connectDoc = nil

    showTeamSelect()
end

function selectTeam(teamId)
    myTeam = teamId

    if netMode == "client" and (not hasReceivedID or not hasReceivedInit or myID == 0) then
        pendingStartAfterInit = true

        if teamSelectDoc then
            local title = RmlUi.getElementById(teamSelectDoc, "team-title")
            if title then
                RmlUi.setInnerRML(title, "Ожидание сервера...")
            end
        end

        return
    end

    if teamSelectDoc then
        RmlUi.destroy(teamSelectDoc)
        teamSelectDoc = nil
    end

    startGame()
end

function findSpawns()
    spawnPointsRed = {}
    spawnPointsBlue = {}
    for _, obj in ipairs(Object.list()) do
        if obj.name == "team_RED_spawn" then
            table.insert(spawnPointsRed, obj.position)
        end
        if obj.name == "team_BLUE_spawn" then
            table.insert(spawnPointsBlue, obj.position)
        end
    end
    if #spawnPointsRed == 0 then
        table.insert(spawnPointsRed, vec3(0, 0, 1))
    end
    if #spawnPointsBlue == 0 then
        table.insert(spawnPointsBlue, vec3(10, 0, 1))
    end
end

function getSpawnPos(team)
    local list = (team == 1) and spawnPointsRed or spawnPointsBlue
    local basePos = list[math.random(1, #list)]
    return vec3(basePos.x + math.random(-1, 1), basePos.y + math.random(-1, 1), basePos.z)
end

function startGame()
	menuCamActive = false

    if netMode == "host" then
        hasReceivedCFG = true
        hasReceivedInit = true
    end

    gameStarted = true
    isPaused = false
    pendingStartAfterInit = false
    preGameInitTimer = 0.0
	hostLastPacketTime = Scene.getTime()
	
	activeClients = {}
    blockedIDs = {}
    lastNetPosTime = 0.0
    hostBeatTimer = 0.0
    hostCheckTimer = 0.0
    pingSendTimer = 0.0

    if pauseDoc then
        RmlUi.destroy(pauseDoc)
        pauseDoc = nil
    end

    switchWeapon(6, true)

    caCurrent = 0.005
    caTarget = 0.005
    caStartValue = 0.005
    caFadeTimer = 0.0

    Graphics.chromaticAberrationStrength = caCurrent
    Camera.setInputMode(InputMode.Game2)

    loadMap(currentMap)

    findDynamicObjects()
    findSpawns()

    local initialSpawn = getSpawnPos(myTeam)

    myChar = spawnLocalR6(initialSpawn)

    safePhysicsCall(Physics.reset, myChar.physRoot.name, initialSpawn)
    safePhysicsCall(Physics.setVelocity, myChar.physRoot.name, vec3(0, 0, 0))
    safePhysicsCall(Physics.activate, myChar.physRoot.name)

    if myChar.anim then
        myChar.anim.position = initialSpawn + vec3(0, 0, 0.5 * R6_SCALE)
    end

    applySkinToChar(myChar, mySkin)
    
    setTeamColor(myChar.body, myTeam)

    Physics.setFriction(myChar.physRoot.name, 0.0)
    Physics.setRestitution(myChar.physRoot.name, 0.0)

    createViewmodelsForWeapon(currentWeapon)

    currentAmmo = maxAmmo
    isReloading = false
    isDead = false

    roundActive = true
    roundCooldown = true

    killsThisRound = {}
    firstKillerID = 0
    spectateTargetID = 0
    roundTime = 0.0

	spawn(function()
		wait(2.0)

		if gameStarted and netMode ~= "none" then
			roundCooldown = false
		end
	end, "cdTask")

    spawn(function()
        while netMode ~= "none" do
            wait(2.0)

            Network.send(string.format("SKIN %d %s", myID, mySkin))
            Network.send(string.format("NAME %d %s", myID, myName))

            if netMode == "host" then
                Network.send(string.format("INIT %s", currentMap))

                local rDef = restoreDefaultWeaponsOnRoundStart and 1 or 0
                local rMon = restorMoneyOnRoundStart and 1 or 0
                local rWep = removeWeaponsOnDeath and 1 or 0

                Network.send(string.format("MODE %s", gameMode))

                Network.send(string.format(
                    "CFG %d %d %f %d %d %f %d %d %d %d %d %d %d",
                    startMoney, maxMoney, roundBuyTime,
                    rDef, rMon, restoreOldWeaponCostMultipiler,
                    roundWonMoney, roundLoseMoney, killMoney, deathMoney,
                    rWep, roundsToWin, tieRound
                ))
            end
        end
    end, "skinBroadcast")

    local ppos = Physics.getPosition(myChar.physRoot.name)

    Camera.setPosition(ppos + vec3(0, 0, eyeOffset))
    Camera.lookAt(ppos + vec3(0, 0, eyeOffset) + vec3(1, 0, 0))

    spawn(groundCheckTask, "groundCheck")

    hudDoc = RmlUi.loadDocument("scripts/fps_hud.rml")
    if hudDoc then
        RmlUi.show(hudDoc, 0, 0)

        healthTextEl = RmlUi.getElementById(hudDoc, "health-text")
        healthBarEl = RmlUi.getElementById(hudDoc, "health-bar")
        ammoTextEl = RmlUi.getElementById(hudDoc, "ammo-text")
        reloadTextEl = RmlUi.getElementById(hudDoc, "reload-text")
        scoreRedEl = RmlUi.getElementById(hudDoc, "score-red")
        scoreBlueEl = RmlUi.getElementById(hudDoc, "score-blue")
        roundResultEl = RmlUi.getElementById(hudDoc, "round-result")
        mvpContainerEl = RmlUi.getElementById(hudDoc, "mvp-container")
        mvpTextEl = RmlUi.getElementById(hudDoc, "mvp-text")
        mvpTitleEl = RmlUi.getElementById(hudDoc, "mvp-title")
        deathScreenEl = RmlUi.getElementById(hudDoc, "death-screen")
        killerNameEl = RmlUi.getElementById(hudDoc, "killer-name")
        centerImageEl = RmlUi.getElementById(hudDoc, "center-image")
        killNotifEl = RmlUi.getElementById(hudDoc, "kill-notification")

        if roundResultEl then
            RmlUi.setProperty(roundResultEl, "display", "none")
        end

        if mvpContainerEl then
            RmlUi.setProperty(mvpContainerEl, "display", "none")
        end

        if deathScreenEl then
            RmlUi.setProperty(deathScreenEl, "display", "none")
        end

        updateHUD()
    end

    Network.send(string.format("TEAM %d %d", myID, myTeam))
    Network.send(string.format("NAME %d %s", myID, myName))
    Network.send(string.format("SKIN %d %s", myID, mySkin))
    Network.send(string.format("MUSICKIT %d %s", myID, myMusicKit))
    Network.send(string.format("WEAPON %d %d", myID, currentWeapon))
end

-- ==========================================
-- УПРАВЛЕНИЕ РАУНДАМИ
-- ==========================================
function resetRound()
    local wasDead = isDead

	isPumpReloading = false
	isPumping = false
	pumpTimer = 0.0

    caCurrent = 0.005
    caTarget = 0.005
    caStartValue = 0.005
    caFadeTimer = 0.0
    Graphics.chromaticAberrationStrength = caCurrent
	
	flashActive = false
    Graphics.brightness = flashOrigBrightness
    Graphics.blurAmount = flashOrigBlur

    if viewmodelAnim then
        viewmodelAnim.scale = vec3(0.3, 0.3, 0.3)
    end

    myHealth = 200
    isDead = false
    isReloading = false

    if WEAPONS[currentWeapon] and WEAPONS[currentWeapon].fireType == "minigun" then
        stopMinigunSound(WEAPONS[currentWeapon])
    end
    isCharging = false
    isMinigunLooping = false
    burstShotsRemaining = 0

    if restorMoneyOnRoundStart or not hasReceivedCFG then
        money = startMoney
    end
    if money > maxMoney then
        money = maxMoney
    end
    if money < 0 then
        money = 0
    end

    local shouldResetWeapons = false
    if restoreDefaultWeaponsOnRoundStart then
        shouldResetWeapons = true
    elseif removeWeaponsOnDeath and wasDead then
        shouldResetWeapons = true
    end

    if shouldResetWeapons then
        weaponsSlots = { [1] = nil, [2] = 6, [3] = 13, [4] = nil }
        actualSlotsAmmo = { [1] = nil, [2] = nil, [3] = nil, [4] = nil }
        switchWeapon(6, true)
    else
        actualSlotsAmmo = { [1] = nil, [2] = nil, [3] = nil, [4] = nil }
        if currentWeapon and WEAPONS[currentWeapon] then
            switchWeapon(currentWeapon, true)
        end
    end

    roundActive = true
    roundCooldown = true
    killsThisRound = {}
    firstKillerID = 0
    spectateTargetID = 0
    roundTime = 0.0

	spawn(function()
		wait(2.0)

		if gameStarted and netMode ~= "none" then
			roundCooldown = false
		end
	end, "cdTask")

    clearDeadBodyParts()

    if myChar then
        local spawnPos = getSpawnPos(myTeam)
        safePhysicsCall(Physics.setEnabled, myChar.physRoot.name, true)
        safePhysicsCall(Physics.reset, myChar.physRoot.name, spawnPos)
        safePhysicsCall(Physics.setVelocity, myChar.physRoot.name, vec3(0, 0, 0))

		myChar.body.mesh = "Steve"
		myChar.head.mesh = ""
		myChar.rArm.mesh = ""
		myChar.lArm.mesh = ""
		myChar.rLeg.mesh = "Steve_10"
		myChar.lLeg.mesh = "Steve_6"
		myChar.HeadOut.mesh = ""
		myChar.BodyOut.mesh = "Steve_1"
		myChar.lArmOut.mesh = ""
		myChar.lLegOut.mesh = "Steve_7"
		myChar.rArmOut.mesh = ""
		myChar.rLegOut.mesh = "Steve_11"
		applySkinToChar(myChar, mySkin)
		myChar.head.castShadow = false
		myChar.anim:playClip("idle")
    end
    if viewmodelAnim then
        viewmodelAnim.scale = vec3(0.3, 0.3, 0.3)
    end

    for id, rp in pairs(remotePlayers) do
        rp.isDead = false
        local spawnPos = getSpawnPos(rp.team)
        safePhysicsCall(Physics.setEnabled, rp.physRoot.name, true)
        safePhysicsCall(Physics.setPosition, rp.physRoot.name, spawnPos)
        safePhysicsCall(Physics.activate, rp.physRoot.name)

		rp.body.mesh = "Steve"
		rp.head.mesh = "Steve_2"
		rp.rArm.mesh = "Steve_8"
		rp.lArm.mesh = "Steve_4"
		rp.rLeg.mesh = "Steve_10"
		rp.lLeg.mesh = "Steve_6"
		rp.HeadOut.mesh = "Steve_3"
		rp.BodyOut.mesh = "Steve_1"
		rp.lArmOut.mesh = "Steve_5"
		rp.lLegOut.mesh = "Steve_7"
		rp.rArmOut.mesh = "Steve_9"
		rp.rLegOut.mesh = "Steve_11"
		applySkinToChar(rp, rp.skin)
		rp.head.castShadow = true
		rp.anim:playClip("idle")
    end

    if roundResultEl then
        RmlUi.setProperty(roundResultEl, "display", "none")
    end
    if mvpContainerEl then
        RmlUi.setProperty(mvpContainerEl, "display", "none")
    end
    if deathScreenEl then
        RmlUi.setProperty(deathScreenEl, "display", "none")
    end
    if killerNameEl then
        RmlUi.setInnerRML(killerNameEl, "")
    end
    if hudDoc then
        updateHUD()
    end
end

-- ==========================================
-- ПЕРЕЗАРЯДКА
-- ==========================================
function reloadTask()
    if isReloading or isDead then return end
    local w = WEAPONS[currentWeapon]
    if not w then return end

    -- Если оружие использует помповую перезарядку (по одному патрону)
    if w.reloadType == "pump" then
        if currentAmmo == w.maxAmmo or isPumpReloading then return end
        pumpReloadTask()
        return
    end

    -- Стандартная перезарядка (замена магазина)
    if currentAmmo == w.maxAmmo then return end
    isReloading = true
    if viewmodelAnim then
        pcall(function() viewmodelAnim:playClip("reload") end)
    end
    updateHUD()
    Network.send(string.format("RELOAD %d", myID))

    if w.reloadSFX then w.reloadSFX() end

    currentAmmo = w.maxAmmo
    isReloading = false
    updateHUD()
end

function pumpReloadTask()
    if isPumpReloading or isReloading or isPumping then return end
    local w = WEAPONS[currentWeapon]
    if not w or w.reloadType ~= "pump" then return end
    if currentAmmo >= w.maxAmmo then return end

    isPumpReloading = true
    local interval = w.pumpReloadInterval or 0.6

    while currentAmmo < w.maxAmmo and isPumpReloading do
        -- Добавляем один патрон
        currentAmmo = currentAmmo + 1
        updateHUD()

        -- Воспроизводим звук помпы (локально и в сети)
        if w.pumpSound then
            Sound.play2D(w.pumpSound, 0.5, 1.0, false)
        end
        Network.send(string.format("PUMP %d", myID))

        -- Ждём интервал, но если нас прервали – выходим
        local timeWaited = 0.0
        while timeWaited < interval and isPumpReloading do
            wait(0.05)
            timeWaited = timeWaited + 0.05
            if isDead or not isPumpReloading then break end
        end
        if not isPumpReloading then break end
    end

    isPumpReloading = false
end

function findDynamicObjects()
    dynamicBoxes = {}
    for _, obj in ipairs(Object.list()) do
        if obj.type == "body" and string.sub(obj.name, 1, 8) == "dynamic_" then
            if netMode == "client" then
                Physics.setKinematic(obj.name, true)
            end
            table.insert(dynamicBoxes, obj.name)
        end
    end
end

-- ==========================================
-- УПРАВЛЕНИЕ ОРУЖИЕМ И ЗАКУПКОЙ
-- ==========================================
function switchWeapon(weaponId, forceSwitch)
    local w = WEAPONS[weaponId]
    if not w then return end

    currentSpreadMult = 0.0
    currentRecoilMult = 0.0
	isPumpReloading = false
	isPumping = false
	pumpTimer = 0.0

    if currentWeapon and WEAPONS[currentWeapon] then
        actualSlotsAmmo[WEAPONS[currentWeapon].slot] = currentAmmo
    end

    local targetSlot = w.slot
    weaponsSlots[targetSlot] = weaponId
    currentWeapon = weaponId

    fireRate = w.fireRate
    spreadAngle = w.spreadAngle
    damageBody = w.damageBody
    damageHead = w.damageHead
    damageArm = w.damageArm
    damageLeg = w.damageLeg
    maxAmmo = w.maxAmmo
    recoilKick = w.recoilKick
    bulletsToFire = w.bulletsToFire
    isReloading = false
    canShoot = true
    burstShotsRemaining = 0
    burstCooldownTimer = 0.0

    walkSpeed = w.walkSpeed or 6.0
    sprintSpeed = w.sprintSpeed or 12.0
	jumpForce = 7.0
	if gameMode == "deathmatch" then
		walkSpeed = walkSpeed * 2
		sprintSpeed = sprintSpeed * 2
		jumpForce = jumpForce * 1.5
	end
    shootRange = w.shootRange or 100.0

    if actualSlotsAmmo[targetSlot] and actualSlotsAmmo[targetSlot] > 0 then
        currentAmmo = actualSlotsAmmo[targetSlot]
    else
        currentAmmo = w.maxAmmo
    end

    -- Обновляем вьюмодель для нового оружия
    createViewmodelsForWeapon(weaponId)

    if hudDoc then
        updateHUD()
    end
    Network.send(string.format("WEAPON %d %d", myID, currentWeapon))
end

function buyWeapon(weaponId)
    local w = WEAPONS[weaponId]
    if not w then return end

    if w.price == 0 then
        switchWeapon(weaponId, true)
        return
    end

    if money < w.price then return end

    local slot = w.slot
    if weaponsSlots[slot] and weaponsSlots[slot] ~= weaponId then
        local oldId = weaponsSlots[slot]
        if WEAPONS[oldId] then
            money = money + WEAPONS[oldId].price * restoreOldWeaponCostMultipiler
        end
    end

    money = money - w.price
    switchWeapon(weaponId, true)
end

function showBuyMenu()
    if buyMenuDoc then
        RmlUi.destroy(buyMenuDoc)
    end
    buyMenuDoc = RmlUi.loadDocument("scripts/buy_menu.rml")
    if not buyMenuDoc then return end
    RmlUi.show(buyMenuDoc, 2, 1)

    Camera.setInputMode(InputMode.None)
	
	isAiming = false
    targetAimFov = defaultFov

    moneyTextEl = RmlUi.getElementById(buyMenuDoc, "money-text")

    for id, w in pairs(WEAPONS) do
        local btn = RmlUi.getElementById(buyMenuDoc, "btn-buy-" .. id)
        if btn then
            RmlUi.addEventListener(btn, "click", function()
                buyWeaponClick(id)
            end)
        end
    end

    updateBuyMenu()
end

function closeBuyMenu()
    isBuyMenuOpen = false

    if buyMenuDoc then
        RmlUi.destroy(buyMenuDoc)
        buyMenuDoc = nil
    end

    if not isDead and not isPaused then
        Camera.setInputMode(InputMode.Game2)
    end
end

function updateBuyMenu()
    if not buyMenuDoc then return end
    if moneyTextEl then
        RmlUi.setInnerRML(moneyTextEl, tostring(money) .. "$")
    end
    for id, w in pairs(WEAPONS) do
        local btn = RmlUi.getElementById(buyMenuDoc, "btn-buy-" .. id)
        if btn then
            local canBuy = money >= w.price
            RmlUi.setProperty(btn, "color", canBuy and "#4CAF50" or "#FF5252")
            RmlUi.setProperty(btn, "opacity", canBuy and "1.0" or "0.5")
        end
    end
end

function buyWeaponClick(weaponId)
    buyWeapon(weaponId)
    updateBuyMenu()
end

-- ==========================================
-- СИСТЕМА СВИСТА ПУЛЬ (WHIZ BY) - СТЕРЕО И ДИСТАНЦИЯ
-- ==========================================
WHIZ_DISTANCE = 2.5
WHIZ_DISTANCE_SQ = WHIZ_DISTANCE * WHIZ_DISTANCE
lastWhizSoundTime = 0.0

function checkBulletWhiz(rayStart, rayEnd)
    if isDead or not myChar then return end
    
    local now = Scene.getTime()
    if now - lastWhizSoundTime < 0.08 then return end
    
    local myPos = Physics.getPosition(myChar.physRoot.name) + vec3(0, 0, 1.2)
    local rsx, rsy, rsz = rayStart.x, rayStart.y, rayStart.z
    local rex, rey, rez = rayEnd.x, rayEnd.y, rayEnd.z
    
    -- ==========================================
    -- 1. AABB EARLY OUT (Самое быстрое отсечение)
    -- ==========================================
    -- Считаем мин/макс вручную. Вызов math.min() в Lua — это прыжок в Си, 
    -- а обычный if отрабатывает на уровне байт-кода Lua за наносекунды.
    local minx, maxx = rsx, rsx
    if rex < minx then minx = rex elseif rex > maxx then maxx = rex end
    if myPos.x < minx - WHIZ_DISTANCE or myPos.x > maxx + WHIZ_DISTANCE then return end
    
    local miny, maxy = rsy, rsy
    if rey < miny then miny = rey elseif rey > maxy then maxy = rey end
    if myPos.y < miny - WHIZ_DISTANCE or myPos.y > maxy + WHIZ_DISTANCE then return end
    
    local minz, maxz = rsz, rsz
    if rez < minz then minz = rez elseif rez > maxz then maxz = rez end
    if myPos.z < minz - WHIZ_DISTANCE or myPos.z > maxz + WHIZ_DISTANCE then return end

    -- ==========================================
    -- 2. ТОЧНАЯ МАТЕМАТИКА (Только если пуля реально рядом)
    -- ==========================================
    local rayDirX, rayDirY, rayDirZ = rex - rsx, rey - rsy, rez - rsz
    local rayLenSq = rayDirX*rayDirX + rayDirY*rayDirY + rayDirZ*rayDirZ
    if rayLenSq < 0.0001 then return end
    
    local rayLen = math.sqrt(rayLenSq)
    local invLen = 1.0 / rayLen
    local ndX, ndY, ndZ = rayDirX * invLen, rayDirY * invLen, rayDirZ * invLen
    
    local toMeX, toMeY, toMeZ = myPos.x - rsx, myPos.y - rsy, myPos.z - rsz
    local t = toMeX * ndX + toMeY * ndY + toMeZ * ndZ
    
    if t < 0.0 or t > rayLen then return end
    
    local toMeLenSq = toMeX*toMeX + toMeY*toMeY + toMeZ*toMeZ
    local distSq = toMeLenSq - (t * t)
    
    if distSq < WHIZ_DISTANCE_SQ then
        -- Получаем реальное расстояние (от 0 до WHIZ_DISTANCE)
        local dist = math.sqrt(distSq)
        
        -- Громкость зависит от расстояния: вплотную (0) = 1.0, на границе = 0.0
        local whizVolume = 1.0 - (dist / WHIZ_DISTANCE)
        
        -- Вычисляем точку в пространстве, где пуля пролетела БЛИЖЕ ВСЕГО к нам
        local closestX = rayStart.x + nd.x * t
        local closestY = rayStart.y + nd.y * t
        local closestZ = rayStart.z + nd.z * t
        
        -- Вектор от нас к этой точке пролета
        local toClosestX = closestX - myPos.x
        local toClosestY = closestY - myPos.y
        local toClosestZ = closestZ - myPos.z
        local closestDist = math.sqrt(toClosestX^2 + toClosestY^2 + toClosestZ^2)
        
        local soundPos = myPos
        if closestDist > 0.01 then
            -- Нормализуем направление к точке пролета
            local nx = toClosestX / closestDist
            local ny = toClosestY / closestDist
            local nz = toClosestZ / closestDist
            
            -- Смещаем позицию звука на 0.5 метров В СТОРОНУ пролета пули.
            -- Это заставит 3D аудио движок расставить звук по левому/правому каналу!
            soundPos = vec3(
                myPos.x + nx * 0.5,
                myPos.y + ny * 0.5,
                myPos.z + nz * 0.5
            )
        end
        
        local whizSound = math.random(1, 2) == 1 and "whiz1" or "whiz2"
        local whizPitch = math.randf(0.8, 1.2)
        -- Играем как 3D звук. Умножаем громкость на 2.5, так как на расстоянии 0.5м 
        -- сработает естественное затухание 3D аудио движка, и мы это компенсируем.
        Sound.play3D(whizSound, soundPos, whizVolume * 2.5, whizPitch, false)
        
        lastWhizSoundTime = now -- Обновляем время последнего свиста
    end
end

-- ==========================================
-- ОСНОВНОЙ ЦИКЛ
-- ==========================================
function update(dt)
    -- ==========================================
    -- ESC / PAUSE
    -- ==========================================
    local escDown = Input.isKeyDown(Input.Keys.Escape)

    if escDown and not escWasDown then
        if isPaused then
            closePauseMenu()
        elseif isBuyMenuOpen then
            closeBuyMenu()
        elseif gameStarted and not isDead and netMode ~= "none" and myTeam ~= 0 then
            openPauseMenu()
        end
    end

    escWasDown = escDown

    if isDead and isPaused then
        closePauseMenu()
    end

    -- ==========================================
    -- ПЛАВНОЕ ПОКАЧИВАНИЕ КАМЕРЫ В ГЛАВНОМ МЕНЮ
    -- ==========================================
    if menuCamActive then
        local t = Scene.getTime()
        
        -- смещение позиции (синусоида по осям)
        local offsetX = math.sin(t * 0.4) * 0.2
        local offsetY = math.cos(t * 0.3) * 0.1
        local offsetZ = math.sin(t * 0.2) * 0.05
        local newPos = menuCamBasePos + vec3(offsetX, offsetY, offsetZ)
        Camera.setPosition(newPos)
        
        -- небольшой поворот (рысканье)
        local yawOffset = math.sin(t * 0.15) * 0.5   -- амплитуда 0.5 градуса
        local front = menuCamBaseFront
        local cosA = math.cos(math.rad(yawOffset))
        local sinA = math.sin(math.rad(yawOffset))
        local newFront = vec3(
            front.x * cosA - front.y * sinA,
            front.x * sinA + front.y * cosA,
            front.z
        )
        Camera.setFront(newFront)
    end

	-- ==========================================
	-- КЛИЕНТ: ПРОВЕРКА ЖИВ ЛИ ХОСТ
	-- ==========================================
	if netMode == "client" then
		if hostLastPacketTime == 0.0 then
			hostLastPacketTime = Scene.getTime()
		end

		local now = Scene.getTime()
		if now - hostLastPacketTime > HOST_TIMEOUT then
			showHostDisconnected()
			return
		end
	end

	-- ==========================================
	-- PRE-GAME NETWORKING
	-- ==========================================
	if netMode ~= "none" and not gameStarted then
		if netMode == "host" then
			preGameInitTimer = preGameInitTimer + dt
			if preGameInitTimer >= 1.0 then
				preGameInitTimer = 0.0
				sendServerInit()
			end
		end

		if netMode == "client" then
			helloTimer = helloTimer + dt

			if myID == 0 then
				if helloTimer >= 1.0 then
					helloTimer = 0.0
					Network.send(string.format("HELLO %d", clientToken))
				end
			else
				if helloTimer >= 2.0 then
					helloTimer = 0.0
					Network.send(string.format("PING %d", myID))
				end
			end
		end

		processPreGameNetworking()
		return
	end
	
	-- ==========================================
	-- NETWORK KEEPALIVE / CLIENT CLEANUP
	-- ==========================================
	if netMode == "host" and gameStarted then
		-- Хост регулярно шлёт heartbeat, чтобы клиенты не думали, что он умер
		hostBeatTimer = hostBeatTimer + dt
		if hostBeatTimer >= 2.0 then
			hostBeatTimer = 0.0
			Network.send("HB")
		end

		-- Хост проверяет, не отвалился ли кто-то из клиентов
		hostCheckTimer = hostCheckTimer + dt
		if hostCheckTimer >= 1.0 then
			hostCheckTimer = 0.0

			local now = Scene.getTime()

			for id, lastSeen in pairs(activeClients) do
				if now - lastSeen > CLIENT_TIMEOUT then
					hostRemoveClient(id, true)
				end
			end
		end
	end

	if netMode == "client" and gameStarted and myID ~= 0 then
		-- Клиент регулярно пингует хоста, чтобы хост видел, что клиент жив
		pingSendTimer = pingSendTimer + dt
		if pingSendTimer >= 1.0 then
			pingSendTimer = 0.0
			Network.send(string.format("PING %d", myID))
		end
	end

	-- ==========================================
	-- КЛИЕНТ В ИГРЕ: PING ХОСТУ
	-- Особенно важно, когда игрок мёртв и не шлёт POS.
	-- ==========================================
	if netMode == "client" and myID ~= 0 then
		pingTimer = pingTimer + dt

		if pingTimer >= 2.0 then
			pingTimer = 0.0
			Network.send(string.format("PING %d", myID))
		end
	end

    if roundActive and not roundCooldown and not isDead then
        roundTime = roundTime + dt
    end

    local isInSpawnZone = false
    if myChar then
        local ppos = Physics.getPosition(myChar.physRoot.name)
        local spawnList = (myTeam == 1) and spawnPointsRed or spawnPointsBlue

        for _, sp in ipairs(spawnList) do
            if dist(ppos, sp) < 15.0 then
                isInSpawnZone = true
                break
            end
        end
    end

    -- ==========================================
    -- BUY MENU
    -- Во время паузы закупка недоступна.
    -- ==========================================
    if isPaused then
        bWasDown = false

        if isBuyMenuOpen then
            closeBuyMenu()
            Camera.setInputMode(InputMode.None)
        end
    else
        local bDown = Input.isKeyDown(Input.Keys.B)

        if bDown and not bWasDown then
            if isBuyMenuOpen then
                closeBuyMenu()
            elseif isInSpawnZone and roundTime < roundBuyTime and not isDead then
                isBuyMenuOpen = true
                showBuyMenu()
            end
        end

        if isBuyMenuOpen and (roundTime >= roundBuyTime or not isInSpawnZone or isDead) then
            closeBuyMenu()
        end

        bWasDown = bDown
    end

    if netMode == "none" or myTeam == 0 or not myChar then
        return
    end

    -- ==========================================
    -- DEAD / SPECTATE
    -- ==========================================
    if isDead then
        if isPaused then
            closePauseMenu()
        end

        local newTargetID = 0

        if spectateTargetID ~= 0 and remotePlayers[spectateTargetID] and not remotePlayers[spectateTargetID].isDead then
            newTargetID = spectateTargetID
        else
            local aliveTeammates = {}

            for id, rp in pairs(remotePlayers) do
                if not rp.isDead and rp.team == myTeam then
                    table.insert(aliveTeammates, id)
                end
            end

            if #aliveTeammates > 0 then
                newTargetID = aliveTeammates[math.random(1, #aliveTeammates)]
            end
        end

        if newTargetID ~= spectateTargetID then
            if spectateTargetID ~= 0 and remotePlayers[spectateTargetID] then
                remotePlayers[spectateTargetID].head.mesh = "R6_1"
                remotePlayers[spectateTargetID].head.castShadow = true
            end

            if newTargetID ~= 0 and remotePlayers[newTargetID] then
                remotePlayers[newTargetID].head.mesh = ""
                remotePlayers[newTargetID].head.castShadow = false
            end

            spectateTargetID = newTargetID
        end

        if spectateTargetID ~= 0 and remotePlayers[spectateTargetID] then
            local targetPos = Physics.getPosition(remotePlayers[spectateTargetID].physRoot.name)
            local eyePos = targetPos + vec3(0, 0, eyeOffset)

            Camera.setPosition(eyePos)
            Camera.lookAt(eyePos + vec3(1, 0, 0))
        end
    else
        if spectateTargetID ~= 0 and remotePlayers[spectateTargetID] then
            remotePlayers[spectateTargetID].head.mesh = "R6_1"
            remotePlayers[spectateTargetID].head.castShadow = true
            spectateTargetID = 0
        end

        if roundActive and not roundCooldown then
            if isPaused then
                processPausedMovement(dt)
            else
                processMovement(dt)
                processShooting(dt)
            end
        end
    end

	objSyncTimer = objSyncTimer + dt
	processNetworking()

	-- Если нас выкинуло в меню / потеряли хост / очистили сцену,
	-- дальше лезть нельзя.
	if netMode == "none" or not myChar then
		return
	end
	
	updateBullets(dt)

	cleanupExpiredParticles()

    if not isDead then
        local ppos = Physics.getPosition(myChar.physRoot.name)

        if ppos.z < -100 then
            applyDamageToMe(100, myID)
        end
    end

    -- ==========================================
    -- RELOAD / WEAPON SWITCH
    -- Во время паузы запрещены.
    -- ==========================================
    if isPaused then
        rWasDown = false
    else
        local rDown = Input.isKeyDown(Input.Keys.R)

        if rDown and not rWasDown and not isDead and not roundCooldown then
            spawn(reloadTask, "reloadTask")
        end

        rWasDown = rDown

        if Input.isKeyDown(Input.Keys.Num1) and weaponsSlots[1] then
            switchWeapon(weaponsSlots[1])
        end

        if Input.isKeyDown(Input.Keys.Num2) and weaponsSlots[2] then
            switchWeapon(weaponsSlots[2])
        end

        if Input.isKeyDown(Input.Keys.Num3) and weaponsSlots[3] then
            switchWeapon(weaponsSlots[3])
        end

        if Input.isKeyDown(Input.Keys.Num4) and weaponsSlots[4] then
            switchWeapon(weaponsSlots[4])
        end
    end

    local healthFraction = math.max(0, myHealth / maxHealth)
    caTarget = caBaseAtFullHealth + (caBaseAtZeroHealth - caBaseAtFullHealth) * (1 - healthFraction)

    if caFadeTimer > 0 then
        caFadeTimer = caFadeTimer - dt
        if caFadeTimer < 0 then
            caFadeTimer = 0
        end
        local progress = 1.0 - (caFadeTimer / caFadeDuration)
        caCurrent = caStartValue + (caTarget - caStartValue) * progress
    else
        caCurrent = caTarget
    end
	
	-- ==========================================
    -- ДИНАМИЧЕСКОЕ ВОССТАНОВЛЕНИЕ ОТДАЧИ
    -- ==========================================
    if not isDead and not isBuyMenuOpen then
        local w = WEAPONS[currentWeapon]
        if w then
            local recSpeed = w.recoverySpeed or 4.0
            -- Плавно уменьшаем множители, если не стреляем
            currentSpreadMult = math.max(0.0, currentSpreadMult - dt * recSpeed)
            currentRecoilMult = math.max(0.0, currentRecoilMult - dt * recSpeed)
        end
    end

    Graphics.chromaticAberrationStrength = caCurrent
	
	-- Обновление flash-эффекта (brightness + blur)
    if flashActive then
        flashFadeTimer = flashFadeTimer - dt
        if flashFadeTimer <= 0 then
            flashActive = false
            Graphics.brightness = flashOrigBrightness
            Graphics.blurAmount = flashOrigBlur
        else
            local progress = 1.0 - (flashFadeTimer / flashFadeDuration)
            local currentBrightness = flashOrigBrightness + (flashTargetBrightness - flashOrigBrightness) * (1 - progress)
            local currentBlur = flashOrigBlur + (flashTargetBlur - flashOrigBlur) * (1 - progress)
            Graphics.brightness = currentBrightness
            Graphics.blurAmount = currentBlur
        end
    end
	-- ==========================================
    -- ДИНАМИЧЕСКИЙ FOV И РАЗМЕР КАРТИНКИ
    -- ==========================================
    if centerImageActive then
        centerImageTimer = centerImageTimer + dt
        
        -- 1. Изменение FOV
        local currentFov = 65 - 3 * math.sin(centerImageTimer * 8)
        Camera.setFOV(currentFov)
        
		-- 2. Изменение размера картинки
		if centerImageEl then
		 local currentSizeVh = centerImageBaseVh + centerImageAmpVh * math.sin(centerImageTimer * centerImageSpeed)

			-- Защита от слишком маленького размера
			currentSizeVh = math.max(4.0, currentSizeVh)

			RmlUi.setProperty(centerImageEl, "width", string.format("%.2fvh", currentSizeVh))
			RmlUi.setProperty(centerImageEl, "height", string.format("%.2fvh", currentSizeVh))

			-- Чтобы картинка оставалась по центру, компенсируем половину ширины
			RmlUi.setProperty(centerImageEl, "margin-left", string.format("%.2fvh", -currentSizeVh / 2.0))
		end
    end
    -- ==========================================
    -- АНИМАЦИЯ УВЕДОМЛЕНИЯ О ДЕНЬГАХ
    -- ==========================================
    if killNotifActive then
        killNotifTimer = killNotifTimer + dt
        
        if killNotifPhase == "stay" then
            -- Держим текст 2 секунды
            if killNotifTimer >= 2.0 then
                killNotifPhase = "fade"
                killNotifTimer = 0.0
            end
		elseif killNotifPhase == "fade" then
			-- Плавно улетаем вверх и растворяемся 1 секунду
			local fadeDuration = 1.0
			local progress = killNotifTimer / fadeDuration

			if progress >= 1.0 then
				killNotifActive = false
				RmlUi.setProperty(killNotifEl, "opacity", "0")
				RmlUi.setInnerRML(killNotifEl, "")
			else
				local currentOpacity = 1.0 - progress
				local currentMarginVh = killNotifBaseMarginVh - (progress * killNotifFadeMoveVh)

				RmlUi.setProperty(killNotifEl, "opacity", string.format("%.2f", currentOpacity))
				RmlUi.setProperty(killNotifEl, "margin-top", string.format("%.2fvh", currentMarginVh))
			end
		end
    end
end

-- ==========================================
-- АНИМАЦИИ
-- ==========================================
function updateLocalAnimation()
    myChar.anim.position = Physics.getPosition(myChar.physRoot.name) + vec3(0, 0, 0.5 * R6_SCALE)
    myChar.anim.rotation = vec3(0, 0, camYaw) --addass
    myChar.anim.scale = vec3(R6_SCALE, R6_SCALE, R6_SCALE)

    local vel = Physics.getVelocity(myChar.physRoot.name)
    local speed = math.sqrt(vel.x * vel.x + vel.y * vel.y)
    local targetClip = "idle"
    if speed > 8.0 then
        targetClip = "walk" --"run"
    elseif speed > 1.0 then
        targetClip = "walk"
    end
    if targetClip ~= myChar.currentMoveState then
        myChar.anim:playClip(targetClip)
        myChar.currentMoveState = targetClip
    end
end

function updateRemoteAnimation(id)
    local rp = remotePlayers[id]
    if not rp then return end
    rp.anim.position = Physics.getPosition(rp.physRoot.name) + vec3(0, 0, 0.5 * R6_SCALE)
    rp.anim.rotation = vec3(rp.physRoot.rotation.x, rp.physRoot.rotation.y, rp.physRoot.rotation.z) --addass
    rp.anim.scale = vec3(R6_SCALE, R6_SCALE, R6_SCALE)

    local vel = Physics.getVelocity(rp.physRoot.name)
    local speed = math.sqrt(vel.x * vel.x + vel.y * vel.y)

    local targetClip = "idle"
    if speed > 8.0 then
        targetClip = "walk" --"run"
    elseif speed > 1.0 then
        targetClip = "walk"
    end
    if targetClip ~= rp.currentMoveState then
        rp.anim:playClip(targetClip)
        rp.currentMoveState = targetClip
    end

    if speed > 8.0 then
        local stepDelay = getStepDelay(speed)
        if Scene.getTime() - rp.lastFootstepTime >= stepDelay then
            rp.lastFootstepTime = Scene.getTime()
            local pos = Physics.getPosition(rp.physRoot.name)
            Sound.play3D("footstepConcrete", pos, 2.0, math.randf(0.85, 1.15), false)
        end
    end
end

-- ==========================================
-- СЕТЬ
-- ==========================================
function processNetworking()
    local now = Scene.getTime()

    -- POS отправляем не каждый кадр, а с ограниченной частотой
    if gameStarted and myChar and not isDead then
        if now - lastNetPosTime >= NET_POS_RATE then
            lastNetPosTime = now

            local ppos = Physics.getPosition(myChar.physRoot.name)
            Network.send(string.format("POS %d %.2f %.2f %.2f %.2f %d",
                myID, ppos.x, ppos.y, ppos.z, camYaw, myTeam))
        end
    end

    if netMode == "host" and gameStarted and objSyncTimer >= OBJ_SYNC_RATE then
        for _, boxName in ipairs(dynamicBoxes) do
            local obj = Object.find(boxName)
            if obj then
                local p = obj.position
                local r = obj.rotation
                Network.send(string.format("OBJ %s %.2f %.2f %.2f %.2f %.2f %.2f",
                    boxName, p.x, p.y, p.z, r.x, r.y, r.z))
            end
        end
        objSyncTimer = 0.0
    end

    local packets = Network.receive() or {}

    -- Любой пакет от хоста обновляет таймаут у клиента
    if netMode == "client" and #packets > 0 then
        hostLastPacketTime = now
    end

    for _, packet in ipairs(packets) do
        local parts = {}
        for word in packet:gmatch("%S+") do
            table.insert(parts, word)
        end

        -- Хост отмечает живых клиентов, но только для пакетов, где parts[2] реально ID
        if netMode == "host" then
            local p = parts[1]

            if p == "POS"
                or p == "SHOOT"
                or p == "MNG_CHARGE"
                or p == "MNG_FIRE"
                or p == "MNG_STOP"
                or p == "IMPACT"
                or p == "BLOOD"
                or p == "DEAD"
                or p == "TEAM"
                or p == "NAME"
                or p == "SKIN"
                or p == "MUSICKIT"
                or p == "WEAPON"
                or p == "RELOAD"
                or p == "EXPLOSION"
                or p == "THROW"
                or p == "PUMP"
                or p == "PING"
            then
                local senderID = tonumber(parts[2])

                if senderID and senderID ~= myID then
                    hostTouchClient(senderID, p == "PING" or p == "TEAM" or p == "NAME")
                end
            end
        end

		-- ==========================================
		-- ХОСТ: ОБНОВЛЯЕМ LASTSEEN И ФИЛЬТРУМ ПАКЕТЫ
		-- ==========================================
		if netMode == "host" then
			local senderID = tonumber(parts[2])

			if senderID and parts[1] ~= "HELLO" and activeClients[senderID] then
				activeClients[senderID] = Scene.getTime()
			end

			if senderID and senderID ~= myID and not activeClients[senderID] then
				local p = parts[1]

				if p ~= "HELLO" and p ~= "PING" and p ~= "DISCONNECT" and p ~= "END" then
					goto continue_packet
				end
			end
		end

		if parts[1] == "HELLO" then
			if netMode == "host" then
				handleHello(parts)
			end

		elseif parts[1] == "ASSIGN" and #parts >= 3 then
			if netMode == "client" then
				local id = tonumber(parts[2])
				local token = tonumber(parts[3])

				if id and token == clientToken then
					myID = id
					hasReceivedID = true
				end
			end

		elseif parts[1] == "END" then
			if netMode == "client" then
				showHostDisconnected()
				return
			end

        elseif parts[1] == "DISCONNECT" and #parts == 2 then
            local id = tonumber(parts[2])

            if id then
                -- Если хост сказал, что МЫ отключены -- выходим в меню
                if netMode == "client" and id == myID then
                    showHostDisconnected()
                    return
                end

                if netMode == "host" then
                    if remotePlayers[id] ~= nil or activeClients[id] ~= nil then
                        hostRemoveClient(id, true)
                    else
                        blockedIDs[id] = true
                    end
                else
                    blockedIDs[id] = true
                    removeRemotePlayer(id)
                end
            end

		elseif parts[1] == "POS" and #parts == 7 then
            local id = tonumber(parts[2])
            if id ~= myID then
                local pos = vec3(tonumber(parts[3]), tonumber(parts[4]), tonumber(parts[5]))
                updateRemotePlayer(id, pos, tonumber(parts[6]), tonumber(parts[7]))
                if netMode == "host" then
                    Network.send(packet)
                end
            end
        elseif parts[1] == "OBJ" and #parts == 8 then
            local objName = parts[2]
            local obj = Object.find(objName)
            if obj then
                safePhysicsCall(Physics.setPosition, objName,
                    vec3(tonumber(parts[3]), tonumber(parts[4]), tonumber(parts[5])))
                safePhysicsCall(Physics.setRotation, objName,
                    vec3(tonumber(parts[6]), tonumber(parts[7]), tonumber(parts[8])))
                safePhysicsCall(Physics.activate, objName)
            end
        elseif parts[1] == "SHOOT" and #parts == 8 then
            local id = tonumber(parts[2])
            if id ~= myID then
                local shooterPos = vec3(tonumber(parts[3]), tonumber(parts[4]), tonumber(parts[5]))
                local shootDir = vec3(tonumber(parts[6]), tonumber(parts[7]), tonumber(parts[8]))
                
                local rp = remotePlayers[id]
                local weaponSound = "shot10"
                local isMinigun = false
                local w = nil
                if rp then
                    w = WEAPONS[rp.currentWeapon]
                    if w and w.sound ~= "" then
                        weaponSound = w.sound
                        if w.fireType == "minigun" then isMinigun = true end
                    end
                end
                
                local tStart = shooterPos + vec3(0, 0, eyeOffset)
                
                if not isMinigun then
                    Sound.play3D(weaponSound, tStart, 2.0, math.randf(0.9, 1.1), false)
                end
                
                -- Вспышка
                createFire(tStart + shootDir * 1.2)
                
                -- Свист пули теперь обрабатывается автоматически внутри simulateBulletFlight
                -- для каждого сегмента (включая рикошеты), поэтому старый вызов убираем.
                
                -- Визуальная симуляция полёта пули
                --[[if w and not w.isProjectile then
                    local rayPos = tStart
                    local rayDistance = w.shootRange or 100.0
                    local penetrationsLeft = (w.ammoType == "penetrative") and (w.ammoParameter or 1) or 0
                    local isExplosive = (w.ammoType == "explosive")
                    local instantBullet = w.instantBullet == true
                    
                    -- Для мгновенных пуль (снайперки) рисуем длинный трассер
                    if instantBullet and w.doTracer then
                        local tracerEnd = tStart + shootDir * rayDistance
                        local tracerHit = Physics.raycast(tStart, tracerEnd, {}, ColGroup.WORLD + ColGroup.DYNAMIC + ColGroup.HITBOX)
                        local tracerDist = tracerHit and tracerHit.hit and tracerHit.distance or rayDistance
                        createTracer(tStart, shootDir, tracerDist, 15.0)
                    end
                    
                    FXCounter = FXCounter + 1
                    spawn(function()
                        simulateBulletFlight(shootDir, rayPos, rayDistance, penetrationsLeft, isExplosive, w, instantBullet, true)
                    end, "remote_bullet_" .. id .. "_" .. FXCounter)
                end]]
				if w and not w.isProjectile then
					local rayPos = tStart
                    local rayDistance = w.shootRange or 100.0
                    local penetrationsLeft = (w.ammoType == "penetrative") and (w.ammoParameter or 1) or 0
                    local isExplosive = (w.ammoType == "explosive")
                    local instantBullet = w.instantBullet == true
                    
                    -- 🔥 ЗАМЕНА: Добавляем визуальную пулю в менеджер (isVisual = true)
                    spawnBullet(shootDir, rayPos, rayDistance, penetrationsLeft, isExplosive, w, instantBullet, true, id, rp.team)
                end
            end
            if netMode == "host" then
                Network.send(packet)
            end
        elseif parts[1] == "MNG_CHARGE" and #parts == 2 then
            local id = tonumber(parts[2])
            if id ~= myID and remotePlayers[id] then
                local rp = remotePlayers[id]
                local w = WEAPONS[rp.currentWeapon]
                if w and w.chargeSound ~= "" then
                    local pos = Physics.getPosition(rp.physRoot.name)
                    Sound.play3D(w.chargeSound, pos, 2.0, 1.0, false)
                end
            end
            if netMode == "host" then
                Network.send(packet)
            end
        elseif parts[1] == "MNG_FIRE" and #parts == 5 then
            local id = tonumber(parts[2])
            if id ~= myID and remotePlayers[id] then
                local rp = remotePlayers[id]
                local w = WEAPONS[rp.currentWeapon]
                if w and w.fireSoundType == "minigun" and w.sound ~= "" then
                    local pos = vec3(tonumber(parts[3]), tonumber(parts[4]), tonumber(parts[5]))
                    if rp.minigunLoopSoundId then
                        Sound.stop(rp.minigunLoopSoundId)
                    end
                    rp.minigunLoopSoundId = Sound.play3D(w.sound, pos, 2.0, 1.0, true)
                end
            end
            if netMode == "host" then
                Network.send(packet)
            end
        elseif parts[1] == "MNG_STOP" and #parts == 2 then
            local id = tonumber(parts[2])
            if id ~= myID and remotePlayers[id] then
                local rp = remotePlayers[id]
                local w = WEAPONS[rp.currentWeapon]
                if rp.minigunLoopSoundId then
                    Sound.stop(rp.minigunLoopSoundId)
                    rp.minigunLoopSoundId = nil
                end
                if w and w.stopSound ~= "" then
                    local pos = Physics.getPosition(rp.physRoot.name)
                    Sound.play3D(w.stopSound, pos, 2.0, 1.0, false)
                end
            end
            if netMode == "host" then
                Network.send(packet)
            end
        elseif parts[1] == "IMPACT" and #parts >= 10 then
            -- Визуальные эффекты теперь создаются локально через simulateBulletFlight
            if netMode == "host" then
                Network.send(packet)
            end
        elseif parts[1] == "BLOOD" and #parts >= 8 then
            -- Визуальные эффекты теперь создаются локально через simulateBulletFlight
            if netMode == "host" then
                Network.send(packet)
            end
        elseif parts[1] == "HIT" and #parts == 5 then
            local killerID = tonumber(parts[2])
            local targetID = tonumber(parts[3])
            local dmg = tonumber(parts[4])
            local isHeadshot = (tonumber(parts[5]) == 1)
            
            if targetID == myID then
                applyDamageToMe(dmg, killerID, isHeadshot)
            else
                local rp = remotePlayers[targetID]
                if rp and not rp.isDead then
                    local pos = Physics.getPosition(rp.physRoot.name)
                    Sound.play3D("hurt" .. math.random(1, 3), pos, 8.0, math.randf(0.9, 1.1), false)
                end
            end
            if netMode == "host" then
                Network.send(packet)
            end
        elseif parts[1] == "DEAD" and #parts == 4 then
            local targetID = tonumber(parts[2])
            local killerID = tonumber(parts[3])
            local isHeadshot = (tonumber(parts[4]) == 1)

            if killerID == myID and targetID ~= myID then
                onLocalKill(targetID, isHeadshot)
            end

            if not (netMode == "host" and targetID == myID) then
                addKillScore(killerID, targetID)
            end

            if targetID == myID then
                money = money + deathMoney
            elseif killerID == myID then
                money = money + killMoney
            end
            if money > maxMoney then
                money = maxMoney
            end
            if money < 0 then
                money = 0
            end

			if netMode == "host" then
				if gameMode == "deathmatch" then
					if targetID ~= myID and isPlayerActive(targetID) then
						respawnTokens[targetID] = (respawnTokens[targetID] or 0) + 1
						local token = respawnTokens[targetID]

						spawn(function()
							wait(2.0)

							if not gameStarted or netMode == "none" then
								return
							end

							if respawnTokens[targetID] ~= token then
								return
							end

							if not isPlayerActive(targetID) then
								return
							end
							
							if blockedIDs[targetID] then
								return
							end

							if remotePlayers[targetID] then
								respawnRemotePlayer(targetID)
							end

							Network.send(string.format("RESPAWN %d", targetID))
						end, "respawn_" .. tostring(targetID) .. "_" .. tostring(token))
					end
				else
					handlePlayerDeath(targetID, killerID)
				end

				Network.send(packet)
                
                if targetID ~= myID and remotePlayers[targetID] then
                    local rp = remotePlayers[targetID]
                    local pos = Physics.getPosition(rp.physRoot.name)
                    local rot = rp.physRoot.rotation
                    Sound.play3D("death1", pos, 2.0, 1.0, false)
                    spawnDeadBodyParts(pos, rot, rp.skin, rp.team)
                    Physics.setPosition(rp.physRoot.name, vec3(0, -100, 0))
                    Physics.setEnabled(rp.physRoot.name, false)
					rp.body.mesh = ""
					rp.head.mesh = ""
					rp.rArm.mesh = ""
					rp.lArm.mesh = ""
					rp.rLeg.mesh = ""
					rp.lLeg.mesh = ""
					rp.HeadOut.mesh = ""
					rp.BodyOut.mesh = ""
					rp.lArmOut.mesh = ""
					rp.lLegOut.mesh = ""
					rp.rArmOut.mesh = ""
					rp.rLegOut.mesh = ""
					rp.anim:stop()
                    rp.isDead = true
                end
            elseif targetID ~= myID then
                if remotePlayers[targetID] then
                    local rp = remotePlayers[targetID]
                    local pos = Physics.getPosition(rp.physRoot.name)
                    local rot = rp.physRoot.rotation
                    Sound.play3D("death1", pos, 2.0, 1.0, false)
                    spawnDeadBodyParts(pos, rot, rp.skin, rp.team)
                    safePhysicsCall(Physics.setPosition, rp.physRoot.name, vec3(0, -100, 0))
                    safePhysicsCall(Physics.setEnabled, rp.physRoot.name, false)
					rp.body.mesh = ""
					rp.head.mesh = ""
					rp.rArm.mesh = ""
					rp.lArm.mesh = ""
					rp.rLeg.mesh = ""
					rp.lLeg.mesh = ""
					rp.HeadOut.mesh = ""
					rp.BodyOut.mesh = ""
					rp.lArmOut.mesh = ""
					rp.lLegOut.mesh = ""
					rp.rArmOut.mesh = ""
					rp.rLegOut.mesh = ""
					rp.anim:stop()
                    rp.isDead = true
                end
            end
		elseif parts[1] == "RESPAWN" and #parts == 2 then
            local targetID = tonumber(parts[2])
            if targetID == myID then
                respawnLocalPlayer()
            else
                respawnRemotePlayer(targetID)
            end

        elseif parts[1] == "MODE" and #parts == 2 then
            gameMode = parts[2]
		elseif parts[1] == "ROUND_WIN" then
			roundActive = false
			local winTeam = tonumber(parts[2])
			teamScores[1] = tonumber(parts[3])
			teamScores[2] = tonumber(parts[4])
			mvpID = tonumber(parts[5])
			mvpName = parts[6]
			mvpReason = parts[7]
			if mvpName == "" then
				mvpName = "Nobody"
			end
			updateHUD()
			showRoundResult(winTeam)

		elseif parts[1] == "MATCH_OVER" then
			roundActive = false
			local winTeam = tonumber(parts[2])
			teamScores[1] = tonumber(parts[3])
			teamScores[2] = tonumber(parts[4])
			mvpID = tonumber(parts[5])
			mvpName = parts[6]
			mvpReason = parts[7]
			if mvpName == "" then
				mvpName = "Nobody"
			end
			updateHUD()
			showMatchOver(winTeam)
        elseif parts[1] == "ROUND_RESET" then
            resetRound()
        elseif parts[1] == "TEAM" and #parts == 3 then
            local id = tonumber(parts[2])
            local team = tonumber(parts[3])
            if remotePlayers[id] then
                remotePlayers[id].team = team
            end
            if netMode == "host" then
                Network.send(packet)
            end
        elseif parts[1] == "NAME" and #parts >= 3 then
            local id = tonumber(parts[2])
            local name = table.concat(parts, " ", 3, #parts)
            if id ~= myID then
                if remotePlayers[id] then
                    remotePlayers[id].name = name
                else
                    pendingNames[id] = name
                end
            end
            if netMode == "host" then
                Network.send(packet)
            end
		elseif parts[1] == "SKIN" and #parts >= 3 then
			local id = tonumber(parts[2])
			local skin = table.concat(parts, " ", 3, #parts)
			if id ~= myID then
				if remotePlayers[id] then
					remotePlayers[id].skin = skin
					applySkinToChar(remotePlayers[id], skin) -- Красим ВСЕ части сразу
				else
					pendingSkins[id] = skin
				end
			end
			if netMode == "host" then
				Network.send(packet)
			end
        elseif parts[1] == "MUSICKIT" and #parts >= 3 then
            local id = tonumber(parts[2])
            if id ~= myID and remotePlayers[id] then
                remotePlayers[id].musicKit = table.concat(parts, " ", 3, #parts)
            end
            if netMode == "host" then
                Network.send(packet)
            end
        elseif parts[1] == "WEAPON" and #parts == 3 then
            local id = tonumber(parts[2])
            local weaponId = tonumber(parts[3])
            if id ~= myID and remotePlayers[id] then
                remotePlayers[id].currentWeapon = weaponId
            end
            if netMode == "host" then
                Network.send(packet)
            end
        elseif parts[1] == "RELOAD" and #parts == 2 then
            local id = tonumber(parts[2])
            if id ~= myID and remotePlayers[id] then
                local rp = remotePlayers[id]
                local w = WEAPONS[rp.currentWeapon]
                if w and w.reloadSFX3D then
                    local pos = Physics.getPosition(rp.physRoot.name)
                    spawn(function()
                        w.reloadSFX3D(pos)
                    end)
                end
            end
            if netMode == "host" then
                Network.send(packet)
            end
		elseif parts[1] == "EXPLOSION" and #parts == 6 then
			local id = tonumber(parts[2])
			local pos = vec3(tonumber(parts[3]), tonumber(parts[4]), tonumber(parts[5]))
			local force = tonumber(parts[6])
			if id ~= myID then
				handleExplosion(pos, force, id)
			end
			if netMode == "host" then
				Network.send(packet)
			end
		elseif parts[1] == "THROW" and #parts >= 10 then
			local id = tonumber(parts[2])
			if id ~= myID then
				local pos = vec3(tonumber(parts[3]), tonumber(parts[4]), tonumber(parts[5]))
				local vel = vec3(tonumber(parts[6]), tonumber(parts[7]), tonumber(parts[8]))
				local timer = tonumber(parts[9])
				local type = parts[10]
				local force = tonumber(parts[11]) or 100.0 -- Безопасный фоллбек
				local spawnTime = Scene.getTime()
				
				FXCounter = FXCounter + 1
				local objName = "Grenade_" .. id .. "_" .. FXCounter
				local obj = Object.new("body", objName)
				obj.mesh = "Cube"
				obj.material = "Asphalt"
				obj.scale = vec3(0.2, 0.2, 0.2)
				obj.position = pos
				Physics.createBody(obj.name, 1.0, vec3(0.1, 0.1, 0.1), "box", nil, nil, ColGroup.DYNAMIC, 15)
				Physics.setVelocity(obj.name, vel)
				Physics.setFriction(obj.name, 0.5)
				Physics.setRestitution(obj.name, 0.3)
				Physics.setCcd(obj.name, 0.1, 0.05)
				
				-- === И ДЛЯ УДАЛЕННЫХ ИГРОКОВ ТОЖЕ ДОБАВЛЯЕМ onCollisionEnter! ===
				obj.onCollisionEnter = function(other, point, speed)
					if Scene.getTime() - spawnTime < 0.15 then return end
					
					if type == "explosive" then
						handleExplosion(point, force, id)
						if netMode == "host" then
							Network.send(string.format("EXPLOSION %d %.2f %.2f %.2f %.2f", id, point.x, point.y, point.z, force))
						end
					elseif type == "smoke" then
						createSmoke(point)
						if netMode == "host" then
							Network.send(string.format("SMOKE %.2f %.2f %.2f", point.x, point.y, point.z))
						end
					elseif type == "flashbang" then
						handleFlashbang(point, id)
						if netMode == "host" then
							Network.send(string.format("FLASH %.2f %.2f %.2f", point.x, point.y, point.z))
						end
					elseif type == "warpGrenade" then
						createWarp(point, id)
						if netMode == "host" then
							Network.send(string.format("WARP %.2f %.2f %.2f %d", point.x, point.y, point.z, id))
						end
					end
					safePhysicsCall(Physics.destroyBody, objName)
					pcall(obj.destroy, obj)
				end

				spawn(function()
					projectileTimerTask(objName, id, timer, type, force)
				end, "projTask_" .. objName)
			end
			if netMode == "host" then
				Network.send(packet)
			end
        elseif parts[1] == "SMOKE" and #parts == 4 then
            local pos = vec3(tonumber(parts[2]), tonumber(parts[3]), tonumber(parts[4]))
            createSmoke(pos)
            if netMode == "host" then
                Network.send(packet)
            end
        elseif parts[1] == "FLASH" and #parts == 4 then
            local pos = vec3(tonumber(parts[2]), tonumber(parts[3]), tonumber(parts[4]))
            handleFlashbang(pos, 0)
            if netMode == "host" then
                Network.send(packet)
            end
		elseif parts[1] == "WARP" and #parts == 5 then
			local pos = vec3(tonumber(parts[2]), tonumber(parts[3]), tonumber(parts[4]))
			local warpShooterID = tonumber(parts[5])
			
			-- Если телепортировался не мы, показываем эффект и двигаем его модель
			if warpShooterID ~= myID then
				createWarpParticles(pos)
				if remotePlayers[warpShooterID] then
					local rp = remotePlayers[warpShooterID]
					safePhysicsCall(Physics.setPosition, rp.physRoot.name, pos + vec3(0, 0, 1.0))
					safePhysicsCall(Physics.setVelocity, rp.physRoot.name, vec3(0, 0, 0))
					safePhysicsCall(Physics.activate, rp.physRoot.name)
				end
			end
			
			if netMode == "host" then
				Network.send(packet)
			end
        elseif parts[1] == "CFG" and #parts == 14 and netMode == "client" then
            startMoney = tonumber(parts[2])
            maxMoney = tonumber(parts[3])
            roundBuyTime = tonumber(parts[4])
            restoreDefaultWeaponsOnRoundStart = tonumber(parts[5]) == 1
            restorMoneyOnRoundStart = tonumber(parts[6]) == 1
            restoreOldWeaponCostMultipiler = tonumber(parts[7])
            roundWonMoney = tonumber(parts[8])
            roundLoseMoney = tonumber(parts[9])
            killMoney = tonumber(parts[10])
            deathMoney = tonumber(parts[11])
            removeWeaponsOnDeath = tonumber(parts[12]) == 1
            roundsToWin = tonumber(parts[13])
            tieRound = tonumber(parts[14])

            if not hasReceivedCFG then
                money = startMoney
                hasReceivedCFG = true
            end

            if money > maxMoney then
                money = maxMoney
            end
            if money < 0 then
                money = 0
            end

            if isBuyMenuOpen then
                updateBuyMenu()
            end
            if hudDoc then
                updateHUD()
            end
		elseif parts[1] == "PUMP" and #parts == 2 then
            local id = tonumber(parts[2])
            if id ~= myID and remotePlayers[id] then
                local rp = remotePlayers[id]
                local pos = Physics.getPosition(rp.physRoot.name)
                local w = WEAPONS[rp.currentWeapon]
                if w and w.pumpSound then
                    Sound.play3D(w.pumpSound, pos, 2.0, 1.0, false)
                end
            end
            if netMode == "host" then
                Network.send(packet)   -- ретранслируем всем клиентам
            end
        end
		::continue_packet::
    end
end

function updateRemotePlayer(id, pos, yaw, team)
    if blockedIDs[id] then
        return
    end
    if not remotePlayers[id] then
        local rp = spawnRemoteR6(id, pos)
        local pName = pendingNames[id] or ("Player_" .. id)
        local pSkin = pendingSkins[id] or "navalniySkin1"
        pendingNames[id] = nil
        pendingSkins[id] = nil
        rp.name = pName
        rp.skin = pSkin
        rp.body.material = pSkin
        rp.team = team or 1
        remotePlayers[id] = rp
    end
    remotePlayers[id].team = team or remotePlayers[id].team
    setTeamColor(remotePlayers[id].body, remotePlayers[id].team)
    if not remotePlayers[id].isDead then
        local rootObj = Object.find(remotePlayers[id].physRoot.name)
        if rootObj then
            safePhysicsCall(Physics.setPosition, remotePlayers[id].physRoot.name, pos)
            safePhysicsCall(Physics.setRotation, remotePlayers[id].physRoot.name, vec3(0, 0, yaw))
            safePhysicsCall(Physics.activate, remotePlayers[id].physRoot.name)
        end
        updateRemoteAnimation(id)
    end
end

-- ==========================================
-- ОБРАБОТКА СМЕРТИ, РАУНДОВ, MVP
-- ==========================================
function checkDeathmatchWin(winTeam)
    if teamScores[1] >= tieRound and teamScores[2] >= tieRound then
        roundActive = false
        local mID, mName, mReason = calculateMVP(0)
        mvpID = mID
        mvpName = mName
        mvpReason = mReason
        Network.send(string.format("MATCH_OVER %d %d %d %d %s %s", 0, teamScores[1], teamScores[2], mvpID, mvpName, mvpReason))
        showMatchOver(0)
    elseif winTeam > 0 and teamScores[winTeam] >= roundsToWin then
        roundActive = false
        local mID, mName, mReason = calculateMVP(winTeam)
        mvpID = mID
        mvpName = mName
        mvpReason = mReason
        Network.send(string.format("MATCH_OVER %d %d %d %d %s %s", winTeam, teamScores[1], teamScores[2], mvpID, mvpName, mvpReason))
        showMatchOver(winTeam)
    end
end

function handlePlayerDeath(targetID, killerID)
    if killerID ~= targetID then
        killsThisRound[killerID] = (killsThisRound[killerID] or 0) + 1
        if firstKillerID == 0 then
            firstKillerID = killerID
        end
    end
    if targetID ~= myID and remotePlayers[targetID] then
        remotePlayers[targetID].isDead = true
    end

    local redAlive = 0
    local blueAlive = 0
    if not isDead then
        if myTeam == 1 then
            redAlive = redAlive + 1
        else
            blueAlive = blueAlive + 1
        end
    end
    for id, rp in pairs(remotePlayers) do
        if not rp.isDead then
            if rp.team == 1 then
                redAlive = redAlive + 1
            else
                blueAlive = blueAlive + 1
            end
        end
    end

    if redAlive == 0 then
        endRound(2)
    elseif blueAlive == 0 then
        endRound(1)
    end
end

function endRound(winTeam)
    roundActive = false
    teamScores[winTeam] = teamScores[winTeam] + 1
    updateHUD()
    local mID, mName, mReason = calculateMVP(winTeam)
    mvpID = mID
    mvpName = mName
    mvpReason = mReason

    if teamScores[1] >= tieRound and teamScores[2] >= tieRound then
        Network.send(string.format("MATCH_OVER %d %d %d %d %s %s", 0, teamScores[1], teamScores[2], mvpID, mName, mReason))
        showMatchOver(0)
    elseif teamScores[winTeam] >= roundsToWin then
        Network.send(string.format("MATCH_OVER %d %d %d %d %s %s", winTeam, teamScores[1], teamScores[2], mvpID, mName, mReason))
        showMatchOver(winTeam)
    else
        Network.send(string.format("ROUND_WIN %d %d %d %d %s %s", winTeam, teamScores[1], teamScores[2], mvpID, mName, mReason))
        showRoundResult(winTeam)
        spawn(roundDelayTask, "roundDelay")
    end
end

function roundDelayTask()
    wait(8.0)

    if not gameStarted or netMode == "none" then
        return
    end

    Network.send("ROUND_RESET")
    resetRound()
end

function calculateMVP(winTeam)
    local bestID = 0
    local bestKills = 0
    local isTied = false

    for id, kills in pairs(killsThisRound) do
        local team = (id == myID) and myTeam or (remotePlayers[id] and remotePlayers[id].team or 0)
        if team == winTeam then
            if kills > bestKills then
                bestKills = kills
                bestID = id
                isTied = false
            elseif kills == bestKills and kills > 0 then
                isTied = true
            end
        end
    end

    if bestID == 0 then
        if myTeam == winTeam then
            bestID = myID
        end
        if bestID == 0 then
            for id, rp in pairs(remotePlayers) do
                if rp.team == winTeam then
                    bestID = id
                    break
                end
            end
        end
    end

    local reason = "first_kill"
    if not isTied and bestKills > 1 then
        reason = "most_kills"
    end

    local name
    if bestID == myID then
        name = myName
    elseif remotePlayers[bestID] then
        name = remotePlayers[bestID].name
    else
        name = "Nobody"
    end

    return bestID, name, reason
end

function showRoundResult(winTeam)
    if winTeam == myTeam then
        money = money + roundWonMoney
    else
        money = money + roundLoseMoney
    end
    if money > maxMoney then
        money = maxMoney
    end
    if money < 0 then
        money = 0
    end
    if hudDoc then
        updateHUD()
    end

    if not mvpContainerEl then return end
    if deathScreenEl then
        RmlUi.setProperty(deathScreenEl, "display", "none")
    end
    if killerNameEl then
        RmlUi.setInnerRML(killerNameEl, "")
    end

    if mvpTitleEl then
        local text = winTeam == 1 and "RED TEAM WINS THE ROUND" or "BLUE TEAM WINS THE ROUND"
        RmlUi.setInnerRML(mvpTitleEl, text)
        RmlUi.setProperty(mvpTitleEl, "color", winTeam == 1 and "#ff4444" or "#4488ff")
    end
    local displayReason = string.gsub(mvpReason, "_", " ")
    if mvpTextEl then
        RmlUi.setInnerRML(mvpTextEl, "MVP: " .. mvpName .. " for " .. displayReason)
    end
    RmlUi.setProperty(mvpContainerEl, "display", "flex")
	local kit = "muskit1_mvp"   -- резерв
	if mvpID == myID then
		kit = myMusicKit
	elseif remotePlayers[mvpID] and remotePlayers[mvpID].musicKit then
		kit = remotePlayers[mvpID].musicKit
	end
	Sound.play2D(kit, 0.4, 1.0, false)
end

function showMatchOver(winTeam)
    if not mvpContainerEl then return end
    if deathScreenEl then
        RmlUi.setProperty(deathScreenEl, "display", "none")
    end
    if killerNameEl then
        RmlUi.setInnerRML(killerNameEl, "")
    end

    if mvpTitleEl then
        local text = ""
        local color = "#ffffff"
        if winTeam == 0 then
            text = "DRAW"
            color = "#ffffff"
        else
            text = winTeam == 1 and "RED TEAM WIN" or "BLUE TEAM WIN"
            color = winTeam == 1 and "#ff4444" or "#4488ff"
        end
        RmlUi.setInnerRML(mvpTitleEl, text)
        RmlUi.setProperty(mvpTitleEl, "color", color)
    end

    local displayReason = string.gsub(mvpReason, "_", " ")
    if mvpTextEl then
        RmlUi.setInnerRML(mvpTextEl, "MVP: " .. mvpName .. " for " .. displayReason)
    end
    RmlUi.setProperty(mvpContainerEl, "display", "flex")
	local kit = "muskit1_mvp"   -- резерв
	if mvpID == myID then
		kit = myMusicKit
	elseif remotePlayers[mvpID] and remotePlayers[mvpID].musicKit then
		kit = remotePlayers[mvpID].musicKit
	end
	Sound.play2D(kit, 0.4, 1.0, false)
    spawn(mvpEndTask, "mvpEnd")
end

function mvpEndTask()
    wait(8.0)

    if not gameStarted or netMode == "none" then
        return
    end

    teamScores = { [1] = 0, [2] = 0 }

    if mvpContainerEl then
        RmlUi.setProperty(mvpContainerEl, "display", "none")
    end

    updateHUD()

    Network.send("ROUND_RESET")
    resetRound()
end

-- ==========================================
-- ПОЛУЧЕНИЕ УРОНА
-- ==========================================
function addKillScore(killerID, targetID)
    if gameMode == "deathmatch" and killerID ~= targetID then
        local killerTeam = 0
        if killerID == myID then
            killerTeam = myTeam
        elseif remotePlayers[killerID] then
            killerTeam = remotePlayers[killerID].team
        end
        
        if killerTeam > 0 then
            teamScores[killerTeam] = (teamScores[killerTeam] or 0) + 1
            if netMode == "host" then
                killsThisRound[killerID] = (killsThisRound[killerID] or 0) + 1
                if firstKillerID == 0 then firstKillerID = killerID end
                checkDeathmatchWin(killerTeam)
            end
            updateHUD()
        end
    end
end

function applyDamageToMe(dmg, killerID, isHeadshot)
    if isHeadshot == nil then isHeadshot = false end
    if isDead or roundCooldown then return end
    if killerID ~= myID then
        local killerTeam = remotePlayers[killerID] and remotePlayers[killerID].team or 0
        if killerTeam == myTeam and myTeam ~= 0 then return end
    end

    myHealth = myHealth - dmg
	Sound.play2D("hurt" .. math.random(1, 3), 3.0, math.randf(0.9, 1.1), false)
    caCurrent = caCurrent + caIncrement
    caStartValue = caCurrent
    caFadeTimer = caFadeDuration
    updateHUD()

	if myHealth <= 0 then
		isDead = true
		
		isAiming = false
		targetAimFov = defaultFov
		currentAimFov = defaultFov

		if isPaused then
			closePauseMenu()
		end

		myHealth = 0
        updateHUD()

        if WEAPONS[currentWeapon] and WEAPONS[currentWeapon].fireType == "minigun" then
            stopMinigunSound(WEAPONS[currentWeapon])
        end

        if myChar then
            safePhysicsCall(Physics.setPosition, myChar.physRoot.name, vec3(0, -100, 0))
            safePhysicsCall(Physics.setEnabled, myChar.physRoot.name, false)
			myChar.body.mesh = ""
			myChar.head.mesh = ""
			myChar.rArm.mesh = ""
			myChar.lArm.mesh = ""
			myChar.rLeg.mesh = ""
			myChar.lLeg.mesh = ""
			myChar.HeadOut.mesh = ""
			myChar.BodyOut.mesh = ""
			myChar.lArmOut.mesh = ""
			myChar.lLegOut.mesh = ""
			myChar.rArmOut.mesh = ""
			myChar.rLegOut.mesh = ""
			myChar.anim:stop()
        end
        if viewmodelAnim then
            viewmodelAnim.scale = vec3(0, 0, 0)
        end

        local deathPos = Physics.getPosition(myChar.physRoot.name)
        local deathRot = myChar.physRoot.rotation
        spawnDeadBodyParts(deathPos, deathRot, mySkin, myTeam)
        Sound.play2D("death1", 1.0, 1.0, false)
        createDeathParticles(deathPos, vec3(0, 0, 1))

        local kName = "Unknown"
        if killerID == myID then
            kName = "Yourself"
        elseif remotePlayers[killerID] then
            kName = remotePlayers[killerID].name
        end
        if killerNameEl then
            RmlUi.setInnerRML(killerNameEl, "KILLED BY " .. kName)
        end
        if deathScreenEl then
            RmlUi.setProperty(deathScreenEl, "display", "flex")
        end

		if netMode == "host" then
			if gameMode == "deathmatch" then
				addKillScore(killerID, myID)

				respawnTokens[myID] = (respawnTokens[myID] or 0) + 1
				local token = respawnTokens[myID]

				spawn(function()
					wait(2.0)

					if respawnTokens[myID] ~= token then
						return
					end

					respawnLocalPlayer()
					Network.send(string.format("RESPAWN %d", myID))
				end, "respawn_host_" .. tostring(token))
			else
				handlePlayerDeath(myID, killerID)
			end
            -- НОВОЕ: добавили флаг хедшота в пакет DEAD
            Network.send(string.format("DEAD %d %d %d", myID, killerID, isHeadshot and 1 or 0))
        else
            Network.send(string.format("DEAD %d %d %d", myID, killerID, isHeadshot and 1 or 0))
        end
    end
end

function respawnLocalPlayer()
    if not myChar then return end
    isDead = false
    myHealth = maxHealth

    local spawnPos = getSpawnPos(myTeam)
    safePhysicsCall(Physics.setEnabled, myChar.physRoot.name, true)
    safePhysicsCall(Physics.reset, myChar.physRoot.name, spawnPos)
    safePhysicsCall(Physics.setVelocity, myChar.physRoot.name, vec3(0, 0, 0))
    safePhysicsCall(Physics.activate, myChar.physRoot.name)

	myChar.body.mesh = "Steve"
	myChar.head.mesh = ""
	myChar.rArm.mesh = ""
	myChar.lArm.mesh = ""
	myChar.rLeg.mesh = "Steve_10"
	myChar.lLeg.mesh = "Steve_6"
	myChar.HeadOut.mesh = ""
	myChar.BodyOut.mesh = "Steve_1"
	myChar.lArmOut.mesh = ""
	myChar.lLegOut.mesh = "Steve_7"
	myChar.rArmOut.mesh = ""
	myChar.rLegOut.mesh = "Steve_11"
	applySkinToChar(myChar, mySkin)
	myChar.head.castShadow = false
	myChar.anim:playClip("idle")

    if WEAPONS[currentWeapon] then
        currentAmmo = WEAPONS[currentWeapon].maxAmmo
    end
    if viewmodelAnim then
        viewmodelAnim.scale = vec3(0.3, 0.3, 0.3)
        pcall(function() viewmodelAnim:playClip("idle") end)
    end

    clearDeadBodyParts()

    -- Возвращаем камеру и курсор
    Camera.setInputMode(InputMode.Game2)
    
    if deathScreenEl then
        RmlUi.setProperty(deathScreenEl, "display", "none")
    end
    if killerNameEl then
        RmlUi.setInnerRML(killerNameEl, "")
    end
    updateHUD()
end

function respawnRemotePlayer(id)
    local rp = remotePlayers[id]
    if not rp then return end
    rp.isDead = false
    local spawnPos = getSpawnPos(rp.team)
    safePhysicsCall(Physics.setEnabled, rp.physRoot.name, true)
    safePhysicsCall(Physics.reset, rp.physRoot.name, spawnPos)
    safePhysicsCall(Physics.setVelocity, rp.physRoot.name, vec3(0, 0, 0))
    safePhysicsCall(Physics.activate, rp.physRoot.name)

	rp.body.mesh = "Steve"
	rp.head.mesh = "Steve_2"
	rp.rArm.mesh = "Steve_8"
	rp.lArm.mesh = "Steve_4"
	rp.rLeg.mesh = "Steve_10"
	rp.lLeg.mesh = "Steve_6"
	rp.HeadOut.mesh = "Steve_3"
	rp.BodyOut.mesh = "Steve_1"
	rp.lArmOut.mesh = "Steve_5"
	rp.lLegOut.mesh = "Steve_7"
	rp.rArmOut.mesh = "Steve_9"
	rp.rLegOut.mesh = "Steve_11"
	applySkinToChar(rp, rp.skin)
	rp.head.castShadow = true
	rp.anim:playClip("idle")
end

-- ==========================================
-- ЭФФЕКТЫ (ПУЛИ, ВЗРЫВЫ, ДЫМ И Т.Д.)
-- ==========================================
local dirtMaterials = { "Asphalt", "Carpet1" }
local metalMaterials = { "VentedMetalWall", "MetalGrill" }
local woodMaterials = { "Wood", "Plywood", "Wallpaper1" }

local function contains(table, val)
    for i = 1, #table do
        if table[i] == val then return true end
    end
    return false
end

function getImpactType(matName)
    if contains(metalMaterials, matName) then
        return "metal"
    elseif contains(woodMaterials, matName) then
        return "wood"
    elseif contains(dirtMaterials, matName) then
        return "dirt"
    else
        return "concrete"
    end
end

function playImpactSound(pos, matName)
    local t = getImpactType(matName)
    if t == "metal" then
        Sound.play3D("metalHit" .. math.random(1, 3), pos, 3.2, math.randf(0.8, 0.9), false)
    elseif t == "wood" then
        Sound.play3D("woodHit" .. math.random(1, 3), pos, 3.2, math.randf(0.8, 0.9), false)
    else
        Sound.play3D("concreteHit" .. math.random(1, 3), pos, 2.4, math.randf(0.8, 0.9), false)
    end
end

function createBulletHole(point, normal, matName)
    local t = getImpactType(matName)
    if t == "metal" or t == "dirt" then return end

    bulletHoleCounter = bulletHoleCounter + 1
    local n = "Hole_" .. bulletHoleCounter
    local o = Object.new("mesh", n)
    o.scale = vec3(0.2, 1.0, 0.2)
    o.mesh = "Plane"
    if t == "wood" then
        o.scale = vec3(0.08, 1.0, 0.08)
        o.material = "woodHit" .. math.random(1, 2)
    else
        o.material = "concreteHit" .. math.random(1, 3)
    end
    o.castShadow = false
    o.position = point + normal * 0.02
    local nx, ny, nz = normal.x, normal.y, normal.z
    o.rotation = vec3(
        math.deg(math.asin(math.max(-1.0, math.min(1.0, nz)))) + 180,
        math.random(-15, 15),
        math.deg(math.atan2(-nx, ny))
    )
    table.insert(bulletHoles, n)
    if #bulletHoles > maxBulletHoles then
        local old = table.remove(bulletHoles, 1)
        local oldO = Object.find(old)
        if oldO then oldO:destroy() end
    end
end

function createImpactParticles(point, normal, matName)
    local impactType = getImpactType(matName)
    local offset = normal * 0.05
    if impactType == "dirt" then
        local s = Object.new("particleemitter", "S_" .. bulletHoleCounter)
        s.position = point + offset
        s.texture = "smoke2"
        s.flipbookGrid = vec2(8, 8)
        s.flipbookFPS = 30
        s.randomStartFrame = true
        s.rate = 0
		s.blendMode = ParticleBlendMode.AlphaBlend
		s.lightInfluence = 0.8
        s.direction = normal
        s.lifetime = vec2(0.4, 0.8)
        s.gravity = vec3(0, 0, 1.0)
        s.spreadAngle = vec2(10, 60)
        s.speed = vec2(0.5, 1.5)
        s.brightness = vec2(0.6, 0.8)
        s.opacity = vec2(0.6, 1.0)
        s.color = Gradient.new(vec4(0.6, 0.5, 0.3, 0.6), vec4(0.4, 0.3, 0.2, 0.0))
        s.size = NumSequence.new(0.35, 1.0)
        s:emit(20)
        
        local sp = Object.new("particleemitter", "SP_" .. bulletHoleCounter)
        sp.position = point + offset
        sp.texture = "shine4"
        sp.rate = 0
        sp.direction = normal
        sp.lifetime = vec2(0.1, 0.35)
        sp.gravity = vec3(0, 0, -9.81)
        sp.spreadAngle = vec2(10, 75)
        sp.speed = vec2(4.0, 10.0)
        sp.brightness = vec2(1.5, 3.0)
        sp.opacity = vec2(2.0, 4.0)
        sp.color = Gradient.new({{0.0, vec4(1, 0.9, 0.3, 1.0)}, {0.7, vec4(1, 0.9, 0.3, 1.0)}, {1.0, vec4(1, 0.2, 0, 0.0)}})
        sp.size = NumSequence.new(0.12, 0.01)
        sp:emit(3)
        
    elseif impactType == "wood" then
        local sm = Object.new("particleemitter", "WS_" .. bulletHoleCounter)
        sm.position = point + offset
        sm.texture = "smoke2"
        sm.blendMode = ParticleBlendMode.AlphaBlend
        sm.flipbookGrid = vec2(8, 8)
        sm.flipbookFPS = 30
        sm.randomStartFrame = true
        sm.rate = 0
		sm.blendMode = ParticleBlendMode.AlphaBlend
		sm.lightInfluence = 0.8
        sm.direction = normal
        sm.lifetime = vec2(0.2, 0.4)
        sm.gravity = vec3(0, 0, 1.0)
        sm.spreadAngle = vec2(0, 45)
        sm.speed = vec2(0.25, 1.0)
        sm.brightness = vec2(0.2, 0.6)
        sm.opacity = vec2(0.4, 0.7)
        sm.color = Gradient.new(vec4(1, 0.6, 0.2, 0.4), vec4(1, 0.7, 0.3, 0.0))
        sm.size = NumSequence.new(0.35, 1.0)
        sm:emit(8)
        
        local sp = Object.new("particleemitter", "WSP_" .. bulletHoleCounter)
        sp.position = point + offset
        sp.texture = "electricity1"
        sp.blendMode = ParticleBlendMode.AlphaBlend
        sp.flipbookGrid = vec2(4, 4)
        sp.flipbookFPS = 10
        sp.randomStartFrame = true
        sp.rate = 0
		sp.blendMode = ParticleBlendMode.AlphaBlend
		sp.lightInfluence = 0.8
        sp.direction = normal
        sp.lifetime = vec2(0.05, 0.2)
        sp.gravity = vec3(0, 0, -1.0)
        sp.spreadAngle = vec2(0, 45)
        sp.speed = vec2(10.0, 25.0)
        sp.brightness = vec2(0.8, 1.0)
        sp.opacity = vec2(5.0, 10.0)
        sp.color = Gradient.new(vec4(1, 0.6, 0.2, 0.4), vec4(1, 0.7, 0.3, 0.0))
        sp.size = NumSequence.new(0.035, 0.1)
        sp:emit(80)
        
    elseif impactType == "metal" then
        local sp = Object.new("particleemitter", "SP_" .. bulletHoleCounter)
        sp.position = point + offset
        sp.texture = "shine4"
        sp.rate = 0
        sp.direction = normal
        sp.lifetime = vec2(0.1, 0.35)
        sp.gravity = vec3(0, 0, -9.81)
        sp.spreadAngle = vec2(5, 40)
        sp.speed = vec2(6.0, 12.0)
        sp.brightness = vec2(3.0, 6.0)
        sp.opacity = vec2(3.0, 5.0)
        sp.color = Gradient.new({{0.0, vec4(0.8, 0.9, 1.0, 1.0)}, {0.5, vec4(1, 0.8, 0.3, 1.0)}, {1.0, vec4(1, 0.2, 0, 0.0)}})
        sp.size = NumSequence.new(0.08, 0.01)
        sp:emit(25)
        
    else -- concrete / default
        local s = Object.new("particleemitter", "S_" .. bulletHoleCounter)
        s.position = point + offset
        s.texture = "smoke2"
        s.flipbookGrid = vec2(8, 8)
        s.flipbookFPS = 30
        s.randomStartFrame = true
        s.rate = 0
		s.blendMode = ParticleBlendMode.AlphaBlend
		s.lightInfluence = 0.8
        s.direction = normal
        s.lifetime = vec2(0.4, 0.8)
        s.gravity = vec3(0, 0, 1.0)
        s.spreadAngle = vec2(0, 45)
        s.speed = vec2(0.25, 1.0)
        s.brightness = vec2(0.8, 1.0)
        s.opacity = vec2(0.4, 0.7)
        s.color = Gradient.new(vec4(1, 1, 1, 0.4), vec4(1, 1, 1, 0.0))
        s.size = NumSequence.new(0.35, 1.0)
        s:emit(20)
        
        local sp = Object.new("particleemitter", "SP_" .. bulletHoleCounter)
        sp.position = point + offset
        sp.texture = "shine4"
        sp.rate = 0
        sp.direction = normal
        sp.lifetime = vec2(0.1, 0.35)
        sp.gravity = vec3(0, 0, -9.81)
        sp.spreadAngle = vec2(10, 75)
        sp.speed = vec2(4.0, 10.0)
        sp.brightness = vec2(1.5, 3.0)
        sp.opacity = vec2(2.0, 4.0)
        sp.color = Gradient.new({{0.0, vec4(1, 0.9, 0.3, 1.0)}, {0.7, vec4(1, 0.9, 0.3, 1.0)}, {1.0, vec4(1, 0.2, 0, 0.0)}})
        sp.size = NumSequence.new(0.12, 0.01)
        sp:emit(15)
        
        local sd = Object.new("particleemitter", "SD_" .. bulletHoleCounter)
        sd.position = point + offset
        sd.texture = "stone1"
		sd.normalmap = "stone1nmap"
        sd.blendMode = ParticleBlendMode.AlphaBlend
        sd.flipbookGrid = vec2(8, 8)
        sd.flipbookFPS = 5
        sd.randomStartFrame = true
        sd.rate = 0
		sd.blendMode = ParticleBlendMode.AlphaBlend
		sd.lightInfluence = 0.8
        sd.direction = normal
        sd.lifetime = vec2(1.0, 2.0)
        sd.gravity = vec3(0, 0, -9.81)
        sd.spreadAngle = vec2(10, 75)
        sd.speed = vec2(1.0, 3.0)
        sd.brightness = vec2(0.7, 1.2)
        sd.opacity = vec2(1.0, 2.0)
        sd.color = Gradient.new({{0.0, vec4(1, 1, 1, 1.0)}, {0.7, vec4(1, 1, 1, 1.0)}, {1.0, vec4(1, 1, 1, 0.0)}})
        sd.size = NumSequence.new(0.1, 0.2)
        sd:emit(3)
    end
end

function createBloodParticles(point, normal)
    bulletHoleCounter = bulletHoleCounter + 1
    local s = Object.new("particleemitter", "BLD_" .. bulletHoleCounter)
    s.position = point + normal * 0.05
    s.blendMode = ParticleBlendMode.AlphaBlend
    s.texture = "shine6_4"
    s.flipbookGrid = vec2(4, 4)
    s.flipbookFPS = 30
    s.randomStartFrame = true
    s.rate = 0
	s.blendMode = ParticleBlendMode.AlphaBlend
	s.lightInfluence = 0.5
    s.direction = normal
    s.lifetime = vec2(0.4, 0.8)
    s.gravity = vec3(0, 0, -8.0)
    s.spreadAngle = vec2(10, 60)
    s.speed = vec2(2.0, 4.0)
    s.brightness = vec2(0.6, 0.8)
    s.opacity = vec2(3.0, 4.0)
    s.color = Gradient.new(vec4(0.8, 0.1, 0.1, 0.8), vec4(0.5, 0.0, 0.0, 0.0))
    s.size = NumSequence.new(0.073, 0.23)
    s:emit(40)
end

function createDeathParticles(point, normal)
    bulletHoleCounter = bulletHoleCounter + 1
    local s = Object.new("particleemitter", "DTH_" .. bulletHoleCounter)
    s.position = point + normal * 0.1
    s.blendMode = ParticleBlendMode.AlphaBlend
    s.texture = "shine6_4"
    s.flipbookGrid = vec2(4, 4)
    s.flipbookFPS = 30
    s.randomStartFrame = true
    s.rate = 0
	s.blendMode = ParticleBlendMode.AlphaBlend
	s.lightInfluence = 0.5
    s.direction = normal
    s.lifetime = vec2(1.0, 2.0)
    s.gravity = vec3(0, 0, -8.0)
    s.spreadAngle = vec2(20, 80)
    s.speed = vec2(10.0, 20.0)
    s.brightness = vec2(0.2, 0.5)
    s.opacity = vec2(3.0, 4.0)
    s.color = Gradient.new(vec4(0.8, 0.1, 0.1, 1.0), vec4(0.5, 0.0, 0.0, 0.0))
    s.size = NumSequence.new(0.7, 2.0)
    s:emit(200)
end

local showTracers = true

-- ==========================================
-- СОЗДАНИЕ СВЕТЯЩЕГОСЯ "КОНЧИКА" ПУЛИ (SingleParticle)
-- ==========================================
function createTracerHead(pos, dir, sizeScale)
    FXCounter = FXCounter + 1
    local objName = "TracerHead_" .. FXCounter
    local obj = Object.new("particle", objName)
    obj.position = pos
    obj.texture = "shine4"
    obj.size = 0.07 * (sizeScale or 1.0)
    obj.brightness = 30.0
    obj.opacity = 3.0
    obj.color = vec4(1.0, 0.95, 0.6, 1.0)
    obj.blendMode = ParticleBlendMode.Additive
    obj.orientation = ParticleOrientation.FacingCamera
    obj.lightInfluence = 0.0
    obj.soft = false
    obj.particleDirection = dir
    return obj
end

function createTracer(startPos, direction, distance, sizeScale)
    if not showTracers then return end
    sizeScale = sizeScale or 1.0
    FXCounter = FXCounter + 1
    local tracer = Object.new("particleemitter", "Tracer_" .. FXCounter)
    tracer.position = startPos
    tracer.direction = direction
    local tracerSpeed = BULLET_SPEED / 3
    local tracerLife = math.max(0.05, distance / tracerSpeed)
    tracer.rate = 0
    tracer.lifetime = vec2(tracerLife, tracerLife)
    tracer.speed = vec2(tracerSpeed, tracerSpeed)
    tracer.spreadAngle = vec2(0, 0)
    tracer.gravity = vec3(0, 0, 0)
    tracer.brightness = vec2(30.0, 30.0)
    tracer.opacity = vec2(3.0, 3.0)
    tracer.color = Gradient.new(vec4(1.0, 0.95, 0.6, 1.0), vec4(1.0, 0.95, 0.6, 1.0))
    tracer.size = NumSequence.new(0.07 * sizeScale, 0.07 * sizeScale)
    tracer.texture = "shine4"
    tracer:emit(1)
    scheduleParticleRemoval(tracer, 15.0)
end

function createFire(pos)
    FXCounter = FXCounter + 1
    local flash = Object.new("particleemitter", "MuzzleFlash_" .. FXCounter)
    flash.position = pos
    flash.texture = "flame1"
    flash.flipbookGrid = vec2(8, 8)
    flash.flipbookFPS = 50
    flash.randomStartFrame = true
    flash.rate = 0
    flash.direction = vec3(0, 0, 1)
    flash.lifetime = vec2(0.04, 0.08)
    flash.gravity = vec3(0, 0, 0)
    flash.spreadAngle = vec2(0, 0)
    flash.speed = vec2(0, 0)
    flash.brightness = vec2(4.0, 8.0)
    flash.opacity = vec2(3.0, 5.0)
    flash.color = Gradient.new({{0.0, vec4(1.0, 0.9, 0.5, 1.0)}, {0.5, vec4(1.0, 0.4, 0.1, 0.8)}, {1.0, vec4(0.5, 0.1, 0.0, 0.0)}})
    flash.size = NumSequence.new(0.02, 0.06)
    flash:emit(1)
    scheduleParticleRemoval(flash, 1.0)
    
    local smoke = Object.new("particleemitter", "MuzzleSmoke_" .. FXCounter)
    smoke.position = pos
    smoke.texture = "smoke2"
    smoke.flipbookGrid = vec2(8, 8)
    smoke.flipbookFPS = 30
    smoke.randomStartFrame = true
    smoke.rate = 0
    smoke.direction = vec3(0, 0, 1)
    smoke.lifetime = vec2(0.2, 0.4)
    smoke.gravity = vec3(0, 0, 1.0)
    smoke.spreadAngle = vec2(10, 30)
    smoke.speed = vec2(0.5, 1.5)
    smoke.brightness = vec2(0.5, 0.8)
    smoke.opacity = vec2(0.5, 0.8)
    smoke.color = Gradient.new(vec4(0.9, 0.9, 0.9, 0.6), vec4(0.5, 0.5, 0.5, 0.0))
    smoke.size = NumSequence.new(0.1, 0.3)
    smoke:emit(5)
    scheduleParticleRemoval(smoke, 2.0)
end

function createSmoke(pos)
    FXCounter = FXCounter + 1
    local ps = Object.new("particleemitter", "SmokeFX_" .. FXCounter)
    ps.position = pos
    ps.blendMode = ParticleBlendMode.AlphaBlend
    ps.texture = "smoke3"
	ps.lightInfluence = 0.8
    ps.flipbookGrid = vec2(8, 8)
    ps.flipbookFPS = 15
    ps.rate = 20
    ps.direction = vec3(0, 0, 1)
    ps.lifetime = vec2(4.0, 6.0)
    ps.gravity = vec3(0, 0, 0.5)
    ps.spreadAngle = vec2(15, 120)
    ps.speed = vec2(0.5, 1.5)
    ps.brightness = vec2(0.5, 0.8)
    ps.opacity = vec2(0.5, 1.0)
    ps.color = Gradient.new(vec4(0.7, 0.7, 0.7, 1.0), vec4(0.5, 0.5, 0.5, 0.0))
    ps.size = NumSequence.new(6.0, 10.0)
    scheduleParticleRemoval(ps, 8.0)
end

function createWarpParticles(pos)
    FXCounter = FXCounter + 1
    local warp = Object.new("particleemitter", "WarpFX_" .. FXCounter)
    warp.position = pos
    warp.texture = "dist3"
    warp.blendMode = ParticleBlendMode.Distortion
    warp.rate = 20
	warp.lightInfluence = 0.4 -- тут влияет на угасание с дальностью
    warp.direction = vec3(0, 0, 1)
    warp.lifetime = vec2(0.5, 1.0)
    warp.gravity = vec3(0, 0, 0)
    warp.spreadAngle = vec2(0, 180)
    warp.speed = vec2(1.0, 4.0)
    warp.brightness = vec2(4.0, 6.0)
    warp.opacity = vec2(5.0, 8.0)
    warp.color = Gradient.new({{0.0, vec4(1, 1, 1, 1.0)}, {0.7, vec4(1, 1, 1, 0.8)}, {1.0, vec4(1, 1, 1, 0.0)}})
    warp.size = NumSequence.new(4.0, 10.0)
    scheduleParticleRemoval(warp, 25.0)
    
    FXCounter = FXCounter + 1
    local smoke = Object.new("particleemitter", "SmokeFX_" .. FXCounter) -- Исправлено имя переменной
    smoke.position = pos
    smoke.blendMode = ParticleBlendMode.AlphaBlend
    smoke.texture = "smoke3"
    smoke.flipbookGrid = vec2(8, 8)
    smoke.flipbookFPS = 15
    smoke.rate = 7
    smoke.direction = vec3(0, 0, 1)
    smoke.lifetime = vec2(4.0, 6.0)
    smoke.gravity = vec3(0, 0, 0.5)
    smoke.spreadAngle = vec2(15, 120)
    smoke.speed = vec2(0.5, 1.5)
    smoke.brightness = vec2(0.5, 0.8)
    smoke.opacity = vec2(0.1, 0.2)
    smoke.color = Gradient.new(vec4(0.7, 0.2, 1.0, 1.0), vec4(0.3, 0.5, 0.5, 0.0))
    smoke.size = NumSequence.new(6.0, 10.0)
    scheduleParticleRemoval(smoke, 25.0)
end

function createWarp(pos, shooterID)
    createWarpParticles(pos)
    Sound.play3D("explosion6", pos, 2.0, 0.5, false) 
end

function handleFlashbang(pos, shooterID)
    if not isDead and myChar then
        local ppos = Physics.getPosition(myChar.physRoot.name)
        local dist = dist(ppos, pos)
        if dist < 20.0 then
            local camPos = Camera.getPosition()
            local camDir = Camera.getFront()
            local toFlash = pos - camPos
            local distToFlash = math.sqrt(toFlash.x^2 + toFlash.y^2 + toFlash.z^2)
            if distToFlash > 0 then
                toFlash = toFlash / distToFlash
                local dot = camDir.x * toFlash.x + camDir.y * toFlash.y + camDir.z * toFlash.z
                if dot > 0.3 then
                    -- Существующий эффект CA
                    caCurrent = 0.5
                    caStartValue = 0.5
                    caFadeTimer = 3.0
                    Sound.play2D("flashbang1", 0.35, 0.5, false)

                    -- ===== НОВЫЙ ЭФФЕКТ: яркость и размытие =====
                    flashOrigBrightness = Graphics.getBrightness() or 0.5
                    flashOrigBlur = Graphics.getBlurAmount() or 0.0
                    flashActive = true
                    flashFadeTimer = flashFadeDuration
                    Graphics.brightness = flashTargetBrightness
                    Graphics.blurAmount = flashTargetBlur
                end
            end
        end
    end
end

function projectileTimerTask(objName, shooterID, timer, type, force)
	wait(timer)
	local obj = Object.find(objName)
	if not obj then return end -- Уже взорвался об стену/игрока через onCollisionEnter
	
	local pos = Physics.getPosition(objName)
	if shooterID == myID then
		if type == "explosive" then
			handleExplosion(pos, force, myID)
			if netMode == "host" then
				Network.send(string.format("EXPLOSION %d %.2f %.2f %.2f %.2f", shooterID, pos.x, pos.y, pos.z, force))
			end
		elseif type == "smoke" then
			createSmoke(pos)
			if netMode == "host" then
				Network.send(string.format("SMOKE %.2f %.2f %.2f", pos.x, pos.y, pos.z))
			end
		elseif type == "flashbang" then
			handleFlashbang(pos, shooterID)
			if netMode == "host" then
				Network.send(string.format("FLASH %.2f %.2f %.2f", pos.x, pos.y, pos.z))
			end
		elseif type == "warpGrenade" then
			createWarp(pos, shooterID)
			if netMode == "host" then
				Network.send(string.format("WARP %.2f %.2f %.2f %d", pos.x, pos.y, pos.z, shooterID))
			end
		end
	end
	safePhysicsCall(Physics.destroyBody, objName)
	pcall(obj.destroy, obj)
end

function throwProjectile(w)
	local camPos = Camera.getPosition()
	local yawRad = math.rad(camYaw)
	local pitchRad = math.rad(camPitch)
	local throwDir = vec3(
		math.cos(yawRad) * math.cos(pitchRad),
		math.sin(yawRad) * math.cos(pitchRad),
		math.sin(pitchRad)
	)
	FXCounter = FXCounter + 1
	local objName = "Grenade_" .. myID .. "_" .. FXCounter
	local obj = Object.new("body", objName)
	obj.mesh = "Cube"
	obj.material = "Asphalt"
	obj.scale = vec3(0.2, 0.2, 0.2)
	obj.position = camPos + throwDir * 0.5
	
	local force = w.explosionForce or 100
	local pType = w.projectileType or "explosive"
	local spawnTime = Scene.getTime()
	
	Physics.createBody(obj.name, 1.0, vec3(0.1, 0.1, 0.1), "box", nil, nil, ColGroup.DYNAMIC, 15)
	Physics.setVelocity(obj.name, throwDir * w.projectileSpeed + vec3(0, 0, 2))
	Physics.setFriction(obj.name, 0.5)
	Physics.setRestitution(obj.name, 0.3)
	Physics.setCcd(obj.name, 0.1, 0.05) -- CCD защищает от прострелов на высокой скорости

	-- === НОВАЯ ФИЧА: Мгновенный взрыв при физическом столкновении! ===
	obj.onCollisionEnter = function(other, point, speed)
		-- Grace period: не взрываемся первые 0.15 секунды, чтобы не задеть себя при броске
		if Scene.getTime() - spawnTime < 0.15 then return end
		
		if pType == "explosive" then
			handleExplosion(point, force, myID) -- Исправлено: передаем myID вместо true, чтобы не наносить урон себе
			if netMode == "host" then
				Network.send(string.format("EXPLOSION %d %.2f %.2f %.2f %.2f", myID, point.x, point.y, point.z, force))
			end
		elseif pType == "smoke" then
			createSmoke(point)
			if netMode == "host" then
				Network.send(string.format("SMOKE %.2f %.2f %.2f", point.x, point.y, point.z))
			end
		elseif pType == "flashbang" then
			handleFlashbang(point, myID)
			if netMode == "host" then
				Network.send(string.format("FLASH %.2f %.2f %.2f", point.x, point.y, point.z))
			end
		elseif pType == "warpGrenade" then
			createWarp(point, myID)
			if netMode == "host" then
				Network.send(string.format("WARP %.2f %.2f %.2f %d", point.x, point.y, point.z, myID))
			end
		end
		
		-- Уничтожаем снаряд сразу после активации эффекта
		safePhysicsCall(Physics.destroyBody, objName)
		pcall(obj.destroy, obj)
	end

	-- Добавили force в конец пакета для синхронизации
	Network.send(string.format("THROW %d %.2f %.2f %.2f %.2f %.2f %.2f %.2f %s %.2f",
		myID, obj.position.x, obj.position.y, obj.position.z,
		throwDir.x * w.projectileSpeed, throwDir.y * w.projectileSpeed,
		throwDir.z * w.projectileSpeed + 2,
		w.projectileTimer, pType, force))
		
	-- Запускаем таймер на случай, если снаряд улетел в бездну и ни с чем не столкнулся
	spawn(function()
		projectileTimerTask(objName, myID, w.projectileTimer, pType, force)
	end, "projTimerTask_" .. objName)
end

function projectileTask(objName, shooterID, timer, impact, type, force)
    local timeLeft = timer
    local prevPos = nil
    local gracePeriod = 0.001 -- Первую миллисекунду игнорируем столкновения
    
    while timeLeft > 0 do
        wait(0.001)
        timeLeft = timeLeft - 0.001
        local obj = Object.find(objName)
        if not obj then return end
        
        local currPos = Physics.getPosition(objName)
        if not prevPos then prevPos = currPos end
        
        -- Проверяем столкновения только если прошёл grace period
        if impact and (timer - timeLeft) >= gracePeriod then
            local hit = Physics.raycast(prevPos, currPos, {}, ColGroup.WORLD)
            if hit and hit.hit then
                if shooterID == myID then
                    if type == "explosive" then
                        handleExplosion(hit.point, force, true)
                        if netMode == "host" then
                            Network.send(string.format("EXPLOSION %d %.2f %.2f %.2f %.2f",
                                shooterID, hit.point.x, hit.point.y, hit.point.z, force))
                        end
                    elseif type == "smoke" then
                        createSmoke(hit.point)
                        if netMode == "host" then
                            Network.send(string.format("SMOKE %.2f %.2f %.2f", hit.point.x, hit.point.y, hit.point.z))
                        end
                    elseif type == "flashbang" then
                        handleFlashbang(hit.point, shooterID)
                        if netMode == "host" then
                            Network.send(string.format("FLASH %.2f %.2f %.2f", hit.point.x, hit.point.y, hit.point.z))
                        end
                    elseif type == "warpGrenade" then
                        createWarp(hit.point, shooterID)
                        if netMode == "host" then
                            Network.send(string.format("WARP %.2f %.2f %.2f %d", hit.point.x, hit.point.y, hit.point.z, shooterID))
                        end
                    end
                end
                safePhysicsCall(Physics.destroyBody, objName)
                pcall(obj.destroy, obj)
                return
            end
        end
        
        prevPos = currPos
    end
    
    -- Если таймер истек (взрыв по времени)
    local obj = Object.find(objName)
    if obj then
        local pos = Physics.getPosition(objName)
        if shooterID == myID then
            if type == "explosive" then
                handleExplosion(pos, force, true)
                if netMode == "host" then
                    Network.send(string.format("EXPLOSION %d %.2f %.2f %.2f %.2f",
                        shooterID, pos.x, pos.y, pos.z, force))
                end
            elseif type == "smoke" then
                createSmoke(pos)
                if netMode == "host" then
                    Network.send(string.format("SMOKE %.2f %.2f %.2f", pos.x, pos.y, pos.z))
                end
            elseif type == "flashbang" then
                handleFlashbang(pos, shooterID)
                if netMode == "host" then
                    Network.send(string.format("FLASH %.2f %.2f %.2f", pos.x, pos.y, pos.z))
                end
            elseif type == "warpGrenade" then
                createWarp(pos, shooterID)
                if netMode == "host" then
                    Network.send(string.format("WARP %.2f %.2f %.2f %d", pos.x, pos.y, pos.z, shooterID))
                end
            end
        end
        safePhysicsCall(Physics.destroyBody, objName)
        pcall(obj.destroy, obj)
    end
end

function createExplosion(pos, sizeScale)
    sizeScale = sizeScale or 1.0
    FXCounter = FXCounter + 1
    
    local ps1 = Object.new("particleemitter", "ExplosionFX1_" .. FXCounter)
    ps1.position = pos
    ps1.texture = "flame2"
    ps1.flipbookGrid = vec2(8, 8)
    ps1.flipbookFPS = 32
    ps1.randomStartFrame = false
    ps1.startRotation = vec2(0, 360)
    ps1.rate = 0
    ps1.direction = vec3(0, 0, 1)
    ps1.lifetime = vec2(2, 2)
    ps1.gravity = vec3(0, 0, 0)
    ps1.spreadAngle = vec2(0, 0)
    ps1.speed = vec2(0, 0)
    ps1.brightness = vec2(2, 4)
    ps1.opacity = vec2(1, 1)
    ps1.color = Gradient.new({{0.0, vec4(1.0, 1.0, 1.0, 1.0)}, {0.5, vec4(1.0, 0.5, 0.2, 1.0)}, {0.8, vec4(0.5, 0.1, 0.0, 1.0)}, {1.0, vec4(0.0, 0.0, 0.0, 0.0)}})
    ps1.size = NumSequence.new(3 * sizeScale, 10 * sizeScale)
    ps1:emit(1)
    scheduleParticleRemoval(ps1, 3.5)
    
    local pf3 = Object.new("particleemitter", "ExplosionFX3_" .. FXCounter)
    pf3.position = pos
    pf3.texture = "flame1"
    pf3.blendMode = ParticleBlendMode.AlphaBlend
    pf3.flipbookGrid = vec2(8, 8)
    pf3.flipbookFPS = 32
    pf3.randomStartFrame = false
    pf3.startRotation = vec2(0, 360)
    pf3.rate = 0
	pf3.lightInfluence = 0.3
    pf3.direction = vec3(0, 0, 1)
    pf3.lifetime = vec2(1.7, 2.0)
    pf3.gravity = vec3(0, 0, 0)
    pf3.spreadAngle = vec2(45, 90)
    pf3.speed = vec2(0.2, 1.0)
    pf3.brightness = vec2(4, 8.5)
    pf3.opacity = vec2(0.5, 1)
    pf3.color = Gradient.new({{0.0, vec4(1.0, 1.0, 1.0, 0.0)}, {0.3, vec4(0.75, 0.75, 0.75, 1.0)}, {1.0, vec4(0.5, 0.5, 0.5, 0.0)}})
    pf3.size = NumSequence.new(3 * sizeScale, 10 * sizeScale)
    pf3:emit(5)
    scheduleParticleRemoval(pf3, 3.5)
    
    local pf4 = Object.new("particleemitter", "ExplosionFX4_" .. FXCounter)
    pf4.position = pos
    pf4.texture = "smoke3"
    pf4.blendMode = ParticleBlendMode.AlphaBlend
    pf4.flipbookGrid = vec2(8, 8)
    pf4.flipbookFPS = 15
    pf4.randomStartFrame = false
    pf4.startRotation = vec2(0, 360)
    pf4.rate = 0
	pf4.lightInfluence = 0.65
    pf4.direction = vec3(0, 0, 1)
    pf4.lifetime = vec2(2.5, 5.0)
    pf4.gravity = vec3(0, 0, 0)
    pf4.spreadAngle = vec2(45, 90)
    pf4.speed = vec2(0.2, 1.0)
    pf4.brightness = vec2(0.5, 2.0)
    pf4.opacity = vec2(0.5, 1)
    pf4.color = Gradient.new({{0.0, vec4(1.0, 1.0, 1.0, 0.0)}, {0.3, vec4(1.0, 1.0, 1.0, 0.0)}, {0.55, vec4(0.75, 0.75, 0.75, 1.0)}, {1.0, vec4(0.5, 0.5, 0.5, 0.0)}})
    pf4.size = NumSequence.new(4 * sizeScale, 12 * sizeScale)
    pf4:emit(5)
    scheduleParticleRemoval(pf4, 6.0)
    
    local pf5 = Object.new("particleemitter", "ExplosionFX5_" .. FXCounter)
    pf5.position = pos
    pf5.texture = "dist3"
    pf5.blendMode = ParticleBlendMode.Distortion
	pf5.lightInfluence = 0.4 -- тут влияет на угасание с дальностью
    pf5.startRotation = vec2(0, 360)
    pf5.rotationSpeed = vec2(10, 30)
    pf5.rate = 0
    pf5.direction = vec3(0, 0, 1)
    pf5.lifetime = vec2(3.5, 8.0)
    pf5.gravity = vec3(0, 0, 0.5)
    pf5.spreadAngle = vec2(45, 90)
    pf5.speed = vec2(0.5, 1.5)
    pf5.brightness = vec2(0.65, 1.0)
    pf5.opacity = vec2(0.1, 0.6)
    pf5.color = Gradient.new({{0.0, vec4(1.0, 1.0, 1.0, 1.0)}, {0.3, vec4(1.0, 1.0, 1.0, 1.0)}, {0.55, vec4(0.75, 0.75, 0.75, 1.0)}, {1.0, vec4(0.5, 0.5, 0.5, 0.0)}})
    pf5.size = NumSequence.new(10 * sizeScale, 15 * sizeScale)
    pf5:emit(20)
    scheduleParticleRemoval(pf5, 15)
end

function handleExplosion(pos, force, shooterID)
    createExplosion(pos, force / 100.0)
	Physics.explode(pos, 10.0 + force / 15.0, 15.0 + force / 20.0)
    local volume = math.min(20.0, 8.0 + force * 0.05)
    Sound.play3D("explosion6", pos, volume, 1.0, false)
    local radius = force * 0.1
    
    -- Урон себе (если мы не стрелок и находимся в радиусе)
    if not isDead and myChar and shooterID ~= myID then
        local ppos = Physics.getPosition(myChar.physRoot.name)
        local d = dist(ppos, pos)
        if d < radius then
            local dmg = math.floor(force * (1.0 - (d / radius)))
            applyDamageToMe(dmg, shooterID)
        end
    end

    -- Урон другим игрокам
    for id, rp in pairs(remotePlayers) do
        if not rp.isDead and id ~= shooterID then
            local ppos = Physics.getPosition(rp.physRoot.name)
            local d = dist(ppos, pos)
            if d < radius then
                local dmg = math.floor(force * (1.0 - (d / radius)))
                Network.send(string.format("HIT %d %d %d", shooterID, id, dmg))
            end
        end
    end
    
    -- Урон стрелку (если он в радиусе и это не самоубийство)
    if shooterID == myID and not isDead and myChar then
        local ppos = Physics.getPosition(myChar.physRoot.name)
        local d = dist(ppos, pos)
        if d < radius then
            local dmg = math.floor(force * (1.0 - (d / radius)))
            applyDamageToMe(dmg, myID)
        end
    end
end
--[[
-- ==========================================
-- СИМУЛЯЦИЯ ПОЛЕТА ПУЛИ (Для instantBullet == false/nil)
-- ==========================================

function simulateBulletFlight(shootDir, rayPos, rayDistance, penetrationsLeft, isExplosive, w, instantBullet, isVisual)
    isVisual = isVisual or false
    local currentRayPos = rayPos
    local currentRayDist = rayDistance
    local currentShootDir = shootDir
    local currentPenetrations = penetrationsLeft
    local finalExplosionPos = nil
    local timePerSegment = 1.0 / BULLET_SPEED

    -- ==========================================
    -- ИНИЦИАЛИЗАЦИЯ ЛЕТЯЩЕГО ТРАССЕРА (SingleParticle)
    -- ==========================================
    local useFlyingTracer = (not instantBullet) and w.doTracer
    local tracerObj = nil
    if useFlyingTracer then
        tracerObj = createTracerHead(currentRayPos, currentShootDir, 1.0)
    end
    
    while currentRayDist > 0.01 do
        -- Если пуля мгновенная, стреляем сразу на всю дистанцию. Если летит — шагами по 1 метру.
        local stepDist = instantBullet and currentRayDist or math.min(BULLET_SEGMENT, currentRayDist)
        local nextPos = currentRayPos + currentShootDir * stepDist
        
        -- 🔊 ПРОФИЛАКТИКА СВИСТА: Проверяем каждый сегмент полёта (включая рикошеты)
        checkBulletWhiz(currentRayPos, nextPos)
        
        local hit = Physics.raycast(currentRayPos, nextPos, {}, ColGroup.WORLD + ColGroup.DYNAMIC + ColGroup.HITBOX)
        
        if not hit or not hit.hit then
            -- ✅ ИСПРАВЛЕНИЕ: Ничего не попали, пуля летит дальше
            currentRayPos = nextPos
            currentRayDist = currentRayDist - stepDist
            
            -- 🚀 ОБНОВЛЯЕМ ПОЗИЦИЮ ТРАССЕРА
            if tracerObj then
                tracerObj.position = currentRayPos
            end
            
            -- Если это взрывная пуля и она долетела до конца, запоминаем точку взрыва
            if isExplosive and currentRayDist <= 0.01 then
                finalExplosionPos = nextPos
            end
            
            -- Ждем, пока пуля пролетит этот сегмент (если она не мгновенная)
            if not instantBullet then
                wait(timePerSegment)
            end
        else
            -- 💥 ПРОИЗОШЛО ПОПАДАНИЕ
            local hitName = hit.objectName or ""
            local partNameCheck = string.match(hitName, "R6_(%a+)_%d+")
            
            -- Игнорируем попадание в корень или в самого себя
            if partNameCheck == "Root" or hitName == playerName then
                currentRayPos = hit.point + currentShootDir * 0.1
                currentRayDist = currentRayDist - hit.distance
                if not instantBullet then wait(0.01) end
            else
                local hitMaterial = hit.material or ""
                local partName, targetIDStr = string.match(hitName, "R6_(%a+)_(%d+)")
                local isPlayerHit = false
                
                if partName and targetIDStr then
                    local targetID = tonumber(targetIDStr)
                    local rp = remotePlayers[targetID]
                    if rp and not rp.isDead and rp.team ~= myTeam then
                        isPlayerHit = true
                        local dmg = damageBody
                        local partId = 0
                        if partName == "Head" then dmg = damageHead; partId = 1
                        elseif partName == "RArm" or partName == "LArm" then dmg = damageArm; partId = 2
                        elseif partName == "RLeg" or partName == "LLeg" then dmg = damageLeg; partId = 3 end
                        
                        createBloodParticles(hit.point, hit.normal)
                        Sound.play2D("hitMarker1", 1.0, 1.0, false)
                        if w.hitSound then
                            Sound.play3D(w.hitSound, hit.point, 2.0, 1.0, false)
                        else
                            Sound.play2D("metalHit1", 0.8, 1.2, false)
                        end
                        
                        if not isVisual then
                            Network.send(string.format("BLOOD %d %.2f %.2f %.2f %.2f %.2f %.2f",
                                myID, hit.point.x, hit.point.y, hit.point.z,
                                hit.normal.x, hit.normal.y, hit.normal.z))
                            Network.send(string.format("HIT %d %d %d %d", myID, targetID, dmg, partId))
                        end
                    end
                end
                
                if isExplosive then
                    finalExplosionPos = hit.point
                    break
                end
                
                if isPlayerHit and currentPenetrations > 0 then
                    currentRayPos = hit.point + currentShootDir * 0.1
                    currentRayDist = currentRayDist - hit.distance
                    currentPenetrations = currentPenetrations - 1
                    if not instantBullet then wait(0.01) end
                else
                    if not isPlayerHit then
                        -- ==========================================
                        -- СИСТЕМА РИКОШЕТОВ
                        -- ==========================================
                        local dot = currentShootDir.x * hit.normal.x + currentShootDir.y * hit.normal.y + currentShootDir.z * hit.normal.z
                        local angleToNormalDeg = math.acos(math.clamp(-dot, -1.0, 1.0)) * (180.0 / math.pi)
                        local isRicochet = angleToNormalDeg > 70.0
                        
                        if isRicochet then
                            if not string.find(hitName, "R6_") and not string.find(hitName, "derbis_") and not string.find(hitName, "dynamic_") then
                                createBulletHole(hit.point, hit.normal, hitMaterial)
                            end
                            playImpactSound(hit.point, hitMaterial)
                            createImpactParticles(hit.point, hit.normal, hitMaterial)
                            local ricochetSound = math.random(1, 2) == 1 and "ricochet1" or "ricochet2"
                            Sound.play3D(ricochetSound, hit.point, 3.0, math.randf(0.8, 1.2), false)
                            if not isVisual then
                                Network.send(string.format("IMPACT %d %.2f %.2f %.2f %.2f %.2f %.2f %s %s",
                                    myID, hit.point.x, hit.point.y, hit.point.z,
                                    hit.normal.x, hit.normal.y, hit.normal.z, hitMaterial, hitName))
                            end
                            if string.find(hitName, "<waterHole>") then createWaterParticles(hit.point, hit.normal)
                            elseif string.find(hitName, "<steamHole>") then createSteamParticles(hit.point, hit.normal) end
                            
                            local reflectedDir = vec3(
                                currentShootDir.x - 2 * dot * hit.normal.x,
                                currentShootDir.y - 2 * dot * hit.normal.y,
                                currentShootDir.z - 2 * dot * hit.normal.z
                            )
                            local reflLen = math.sqrt(reflectedDir.x^2 + reflectedDir.y^2 + reflectedDir.z^2)
                            if reflLen > 0.001 then
                                reflectedDir = vec3(reflectedDir.x / reflLen, reflectedDir.y / reflLen, reflectedDir.z / reflLen)
                            end
                            createTracer(hit.point, reflectedDir, 5.0, 0.5)
                            
                            -- 💥 РИКОШЕТ: Удаляем старый трассер и создаем новый (чуть меньше)
                            if tracerObj then 
                                pcall(tracerObj.destroy, tracerObj) 
                            end
                            tracerObj = createTracerHead(hit.point, reflectedDir, 0.6)
                            
                            currentRayPos = hit.point + reflectedDir * 0.1
                            currentShootDir = reflectedDir
                            currentRayDist = currentRayDist - hit.distance
                            if not instantBullet then wait(0.01) end
                        else
                            if string.find(hitName, "<waterHole>") then createWaterParticles(hit.point, hit.normal)
                            elseif string.find(hitName, "<steamHole>") then createSteamParticles(hit.point, hit.normal) end
                            if not isVisual then
                                Network.send(string.format("IMPACT %d %.2f %.2f %.2f %.2f %.2f %.2f %s %s",
                                    myID, hit.point.x, hit.point.y, hit.point.z,
                                    hit.normal.x, hit.normal.y, hit.normal.z, hitMaterial, hitName))
                            end
                            if not string.find(hitName, "R6_") and not string.find(hitName, "derbis_") and not string.find(hitName, "dynamic_") then
                                createBulletHole(hit.point, hit.normal, hitMaterial)
                            end
                            playImpactSound(hit.point, hitMaterial)
                            createImpactParticles(hit.point, hit.normal, hitMaterial)
                            if netMode == "host" and string.sub(hitName, 1, 8) == "dynamic_" then
                                safePhysicsCall(Physics.applyImpulse, hitName, currentShootDir * 15.0)
                            end
                            
                            -- 🛑 УДАР: Трассер оставляет вспышку и исчезает
                            if tracerObj then
                                local finalTracer = tracerObj
                                spawn(function()
                                    wait(0.05) -- Микро-задержка, чтобы не было резкого щелчка
                                    pcall(finalTracer.destroy, finalTracer)
                                end, "tracer_hit_" .. FXCounter)
                                tracerObj = nil
                            end
                            break -- Пуля застряла/уничтожилась
                        end
                    else
                        break -- Игрок пробит, но penetrations закончились
                    end
                end
            end
        end
    end
    
    -- 🌌 КОНЕЦ ПОЛЕТА: Уничтожаем трассер, если он ещё жив
    if tracerObj then
        local finalTracer = tracerObj
        spawn(function()
            wait(0.05)
            pcall(finalTracer.destroy, finalTracer)
        end, "tracer_end_" .. FXCounter)
    end

    if isExplosive and finalExplosionPos then
        if not isVisual then
            Network.send(string.format("EXPLOSION %d %.2f %.2f %.2f %.2f",
                myID, finalExplosionPos.x, finalExplosionPos.y, finalExplosionPos.z, w.ammoParameter))
            handleExplosion(finalExplosionPos, w.ammoParameter, myID)
        end
    end
end
]]
-- ==========================================
-- BULLET MANAGER (30Hz Логика + Визуал, 1м Шаг)
-- ==========================================
local activeBullets = {}
local bulletAccumulator = 0.0

-- 🔥 НАСТРОЙКИ (Можешь крутить под себя)
local BULLET_UPDATE_RATE = 24.0           -- 30 FPS для физики и визуала
local BULLET_FIXED_DT = 1.0 / BULLET_UPDATE_RATE
local MAX_RAYCAST_STEP = 8.0             -- Макс. длина одного рейкаста (защита от穿透)
local MAX_CATCHUP_TICKS = 2               -- Макс. кол-во тиков при фризе (защита от импульсов)

-- Глобальный апдейт, вызывается из update(dt)
function updateBullets(dt)
    bulletAccumulator = bulletAccumulator + dt
    
    -- 🛡️ ЗАЩИТА ОТ «СПИРАЛИ СМЕРТИ» (Импульсных лагов)
    -- Если был жесткий фриз, мы не будем считать 100 тиков за раз, а обрежем до 3-х
    if bulletAccumulator > BULLET_FIXED_DT * MAX_CATCHUP_TICKS then
        bulletAccumulator = BULLET_FIXED_DT * MAX_CATCHUP_TICKS
    end
    
    -- Выполняем тики ровно 30 раз в секунду
    while bulletAccumulator >= BULLET_FIXED_DT do
        bulletAccumulator = bulletAccumulator - BULLET_FIXED_DT
        simulateBulletsStep()
    end
end

-- Создание новой пули
function spawnBullet(shootDir, rayPos, rayDistance, penetrationsLeft, isExplosive, w, instantBullet, isVisual, shooterID, shooterTeam)
	local bullet = {
		pos = rayPos,
		lastPos = rayPos,
		dir = shootDir,
		speed = BULLET_SPEED,
		distLeft = rayDistance,
		pen = penetrationsLeft,
		isExplosive = isExplosive,
		weapon = w,
		isVisual = isVisual,
		shooterID = shooterID,
		shooterTeam = shooterTeam,
		tracerObj = nil,
		isDead = false,
		finalExplosionPos = nil,
		instant = instantBullet,
		ricochetCount = 0,   -- Счётчик количества рикошетов
		damageMult = 1.0     -- Множитель урона (1.0 = 100%, после 1-го рикошета станет 0.6 и т.д.)
	}
    
    if w.doTracer then
        if instantBullet then
            createTracer(rayPos, shootDir, rayDistance, 1.0)
        else
            bullet.tracerObj = createTracerHead(rayPos, shootDir, 1.0)
        end
    end

    table.insert(activeBullets, bullet)
end

-- Один шаг симуляции (Вызывается строго 30 раз в секунду)
function simulateBulletsStep()
    for i = #activeBullets, 1, -1 do
        local b = activeBullets[i]
        
        if b.isDead then
            -- 💀 Чистка ресурсов
            if b.tracerObj then
                local tracerToDestroy = b.tracerObj
                spawn(function()
                    wait(0.05)
                    pcall(tracerToDestroy.destroy, tracerToDestroy)
                end, "tracer_cleanup_" .. i)
            end
            
            if b.isExplosive and b.finalExplosionPos then
                if not b.isVisual then
                    Network.send(string.format("EXPLOSION %d %.2f %.2f %.2f %.2f",
                        b.shooterID, b.finalExplosionPos.x, b.finalExplosionPos.y, b.finalExplosionPos.z, b.weapon.ammoParameter))
                end
                handleExplosion(b.finalExplosionPos, b.weapon.ammoParameter, b.shooterID)
            end
            
            -- Swap-Remove
            activeBullets[i] = activeBullets[#activeBullets]
            table.remove(activeBullets)
            goto continue
        end

        -- ==========================================
        -- 1. HITSCAN (Мгновенные пули / Снайперки)
        -- ==========================================
        if b.instant then
            local nextPos = b.pos + b.dir * b.distLeft
            --checkBulletWhiz(b.pos, nextPos)
			if b.shooterID ~= myID then
				checkBulletWhiz(b.pos, nextPos)
			end
			b.lastPos = b.pos
            
            local hit = Physics.raycast(b.pos, nextPos, {}, ColGroup.WORLD + ColGroup.DYNAMIC + ColGroup.HITBOX)
            if not hit or not hit.hit then
                b.pos = nextPos
                b.distLeft = 0
            else
                processBulletHit(b, hit)
            end
            
            -- Визуал для hitscan
            if b.tracerObj then b.tracerObj.position = b.pos end
            
            if b.distLeft <= 0.01 and not b.isDead then
                if b.isExplosive then b.finalExplosionPos = b.pos end
                b.isDead = true
            end
            
        -- ==========================================
        -- 2. FLYING BULLET (Летящие пули, 30Hz, Шаг 1 метр)
        -- ==========================================
        else
            -- Считаем, сколько метров пуля должна пролететь за этот 1/30 секунды тик
            local totalDistThisTick = BULLET_SPEED * BULLET_FIXED_DT
            if totalDistThisTick > b.distLeft then totalDistThisTick = b.distLeft end
            
            local distRemaining = totalDistThisTick
            local hitSomething = false
            
            -- 🔥 ГЛАВНАЯ ФИШКА: Дробим полет на отрезки по 1 метру (MAX_RAYCAST_STEP)
            -- Это гарантирует, что пуля НЕ ПРОСТРЕЛИТ тонкие стены, даже на 180 FPS
            while distRemaining > 0.001 and not hitSomething do
                local stepDist = math.min(distRemaining, MAX_RAYCAST_STEP)
                local nextPos = b.pos + b.dir * stepDist
				if b.shooterID ~= myID then
					checkBulletWhiz(b.lastPos, nextPos)
				end
                b.lastPos = b.pos
                
                local hit = Physics.raycast(b.pos, nextPos, {}, ColGroup.WORLD + ColGroup.DYNAMIC + ColGroup.HITBOX)
                
                if not hit or not hit.hit then
                    -- Пролетели кусок в пустоту
                    b.pos = nextPos
                    distRemaining = distRemaining - stepDist
                    b.distLeft = b.distLeft - stepDist
                else
                    -- Во что-то попали!
                    processBulletHit(b, hit)
                    hitSomething = true -- Прерываем полет в этом тике
                end
            end
            
            -- 🎨 ВИЗУАЛ (Обновляется строго здесь, то есть 30 FPS)
            if b.tracerObj and not b.isDead then
                b.tracerObj.position = b.pos
            end
            
            -- Проверка конца дистанции
            if b.distLeft <= 0.01 and not b.isDead then
                if b.isExplosive then b.finalExplosionPos = b.pos end
                b.isDead = true
            end
        end
        
        ::continue::
    end
end

-- Обработка столкновения пули (вынесено, чтобы не раздувать цикл)
function processBulletHit(b, hit)
    local hitName = hit.objectName or ""
    local partNameCheck = string.match(hitName, "R6_(%a+)_%d+")
    
    -- Игнорируем корень или самого стрелка
    if partNameCheck == "Root" or hitName == playerName then
        b.pos = hit.point + b.dir * 0.1
        b.distLeft = b.distLeft - hit.distance
        return
    end
    
    local hitMaterial = hit.material or ""
    local partName, targetIDStr = string.match(hitName, "R6_(%a+)_(%d+)")
    local isPlayerHit = false
    
    -- Проверка попадания в игрока
	if partName and targetIDStr then
		local targetID = tonumber(targetIDStr)
		local rp = remotePlayers[targetID]
		if rp and not rp.isDead and rp.team ~= b.shooterTeam then
			isPlayerHit = true
			
			-- Получаем базовый урон из таблицы оружия
			local baseDmg = b.weapon.damageBody
			local partId = 0
			if partName == "Head" then baseDmg = b.weapon.damageHead; partId = 1
			elseif partName == "RArm" or partName == "LArm" then baseDmg = b.weapon.damageArm; partId = 2
			elseif partName == "RLeg" or partName == "LLeg" then baseDmg = b.weapon.damageLeg; partId = 3 end
			
			-- ПРИМЕНЯЕМ МНОЖИТЕЛЬ ПОСЛЕ РИКОШЕТОВ (округляем вниз до целого числа)
			local dmg = math.floor(baseDmg * b.damageMult)

			createBloodParticles(hit.point, hit.normal)
            Sound.play2D("hitMarker1", 1.0, 1.0, false)
            if b.weapon.hitSound then
                Sound.play3D(b.weapon.hitSound, hit.point, 2.0, 1.0, false)
            else
                Sound.play2D("metalHit1", 0.8, 1.2, false)
            end
            
            if not b.isVisual then
                Network.send(string.format("BLOOD %d %.2f %.2f %.2f %.2f %.2f %.2f",
                    b.shooterID, hit.point.x, hit.point.y, hit.point.z,
                    hit.normal.x, hit.normal.y, hit.normal.z))
                Network.send(string.format("HIT %d %d %d %d", b.shooterID, targetID, dmg, partId))
            end
        end
    end
    
    -- Взрывная пуля
    if b.isExplosive then
        b.finalExplosionPos = hit.point
        b.isDead = true
        return
    end
    
    -- Пробитие игрока (Penetration)
    if isPlayerHit and b.pen > 0 then
        b.pos = hit.point + b.dir * 0.1
        b.distLeft = b.distLeft - hit.distance
        b.pen = b.pen - 1
        return
    end
    
    -- Обычное попадание (в мир или в игрока без пробития)
    if not isPlayerHit then
        -- Система рикошетов
        local dot = b.dir.x * hit.normal.x + b.dir.y * hit.normal.y + b.dir.z * hit.normal.z
        local angleToNormalDeg = math.acos(math.max(-1.0, math.min(1.0, -dot))) * (180.0 / math.pi)
        local isRicochet = angleToNormalDeg > 70.0
        
		if isRicochet then
			-- 1. Увеличиваем счётчик рикошетов
			b.ricochetCount = b.ricochetCount + 1

			-- 2. Если это 4-й рикошет, пуля уничтожается
			if b.ricochetCount >= 4 then
				b.isDead = true
				return
			end

			-- 3. Пуля теряет 40% урона (остаётся 60% от текущего значения)
			b.damageMult = b.damageMult * 0.6

			if not string.find(hitName, "R6_") and not string.find(hitName, "derbis_") and not string.find(hitName, "dynamic_") then
				createBulletHole(hit.point, hit.normal, hitMaterial)
			end
			playImpactSound(hit.point, hitMaterial)
            createImpactParticles(hit.point, hit.normal, hitMaterial)
            
            local ricochetSound = math.random(1, 2) == 1 and "ricochet1" or "ricochet2"
            Sound.play3D(ricochetSound, hit.point, 3.0, math.randf(0.8, 1.2), false)
            
            if not b.isVisual then
                Network.send(string.format("IMPACT %d %.2f %.2f %.2f %.2f %.2f %.2f %s %s",
                    b.shooterID, hit.point.x, hit.point.y, hit.point.z,
                    hit.normal.x, hit.normal.y, hit.normal.z, hitMaterial, hitName))
            end
            
            if string.find(hitName, "<waterHole>") then createWaterParticles(hit.point, hit.normal)
            elseif string.find(hitName, "<steamHole>") then createSteamParticles(hit.point, hit.normal) end
            
            -- Расчет вектора отражения
            local reflectedDir = vec3(
                b.dir.x - 2 * dot * hit.normal.x,
                b.dir.y - 2 * dot * hit.normal.y,
                b.dir.z - 2 * dot * hit.normal.z
            )
            local reflLen = math.sqrt(reflectedDir.x^2 + reflectedDir.y^2 + reflectedDir.z^2)
            if reflLen > 0.001 then
                reflectedDir = vec3(reflectedDir.x / reflLen, reflectedDir.y / reflLen, reflectedDir.z / reflLen)
            end
            
            createTracer(hit.point, reflectedDir, 5.0, 0.5)
            
            -- Обновляем трассер (старый уничтожаем, новый создаем)
            if b.tracerObj then 
                pcall(b.tracerObj.destroy, b.tracerObj) 
            end
            b.tracerObj = createTracerHead(hit.point, reflectedDir, 0.6)
            
            b.pos = hit.point + reflectedDir * 0.1
            b.dir = reflectedDir
            b.distLeft = b.distLeft - hit.distance
            return
        else
            -- Жесткое попадание (без рикошета)
            if string.find(hitName, "<waterHole>") then createWaterParticles(hit.point, hit.normal)
            elseif string.find(hitName, "<steamHole>") then createSteamParticles(hit.point, hit.normal) end
            
            if not b.isVisual then
                Network.send(string.format("IMPACT %d %.2f %.2f %.2f %.2f %.2f %.2f %s %s",
                    b.shooterID, hit.point.x, hit.point.y, hit.point.z,
                    hit.normal.x, hit.normal.y, hit.normal.z, hitMaterial, hitName))
            end
            
            if not string.find(hitName, "R6_") and not string.find(hitName, "derbis_") and not string.find(hitName, "dynamic_") then
                createBulletHole(hit.point, hit.normal, hitMaterial)
            end
            playImpactSound(hit.point, hitMaterial)
            createImpactParticles(hit.point, hit.normal, hitMaterial)
            
            if netMode == "host" and string.sub(hitName, 1, 8) == "dynamic_" then
                safePhysicsCall(Physics.applyImpulse, hitName, b.dir * 15.0)
            end
            
            b.isDead = true
            return
        end
    else
        -- Игрок пробит, но penetrations закончились
        b.isDead = true
        return
    end
end

-- ==========================================
-- СТРЕЛЬБА
-- ==========================================
function processShooting(dt)
    if isDead or isBuyMenuOpen or isPaused then
        return
    end
    if isPumpReloading then return end
    if shootCooldown > 0 then
        shootCooldown = shootCooldown - dt
    end

    local w = WEAPONS[currentWeapon]
    if not w then return end

    local lmbDown = Input.isMouseButtonDown(Input.Mouse.Left)

    -- Миниган
    if w.fireType == "minigun" then
        if lmbDown and not isReloading then
            if currentAmmo > 0 then
                if not isCharging and not isMinigunLooping then
                    isCharging = true
                    chargeTimer = 0.0
                    if w.chargeSound and w.chargeSound ~= "" then
                        Sound.play2D(w.chargeSound, 0.5, 1.0, false)
                    end
                    Network.send(string.format("MNG_CHARGE %d", myID))
                end

                if isCharging then
                    chargeTimer = chargeTimer + dt
                    if chargeTimer >= (w.chargeTime or 0.5) then
                        isCharging = false
                        isMinigunLooping = true
                        if w.fireSoundType == "minigun" and w.sound ~= "" then
                            minigunLoopSoundId = Sound.play2D(w.sound, 0.5, 1.0, true)
                        end
                        local camPos = Camera.getPosition()
                        Network.send(string.format("MNG_FIRE %d %.2f %.2f %.2f",
                            myID, camPos.x, camPos.y, camPos.z))
                    end
                end

                if isMinigunLooping then
                    if shootCooldown <= 0 then
                        fireBullet()
                        if w.fireSoundType == "default" and w.sound ~= "" then
                            Sound.play2D(w.sound, 1.0, math.randf(0.9, 1.1), false)
                        end
                        shootCooldown = w.fireRate or 0.05
                    end
                end
            else
                if isMinigunLooping then
                    stopMinigunSound(w)
                end
                if isCharging then
                    isCharging = false
                end
                if not isReloading then
                    spawn(reloadTask, "autoReloadTask")
                end
            end
        else
            if isCharging then
                isCharging = false
            end
            if isMinigunLooping then
                stopMinigunSound(w)
            end
            if viewmodelAnim then pcall(function() viewmodelAnim:playClip("idle") end) end
        end
        return
    end
	
    -- ===== ДРОБОВИК (Pump-action) =====
    if w.fireType == "shotgun" then
        -- Если идёт помпа – обновляем таймер и блокируем стрельбу
        if isPumping then
            pumpTimer = pumpTimer - dt
            if pumpTimer <= 0 then
                isPumping = false
            end
            return
        end

        -- Если перезарядка по патронам активна, не даём стрелять (уже обработано выше)
        -- if isPumpReloading then return end -- можно убрать, т.к. есть общий return

        local canFire = lmbDown and not lmbWasDown
        if canFire and not isReloading then
            if currentAmmo > 0 then
                fireBullet()

                -- Запускаем помпу
                isPumping = true
                pumpTimer = w.pumpDelay or w.fireRate
                if w.pumpSound then
                    Sound.play2D(w.pumpSound, 0.5, 1.0, false)
                end
                -- Отправляем сетевой пакет о помпе (чтобы другие слышали)
                Network.send(string.format("PUMP %d", myID))
            else
                spawn(reloadTask, "autoReloadTask")
            end
        end
        lmbWasDown = lmbDown
        return
    end

    -- Очередь (burst)
    if burstShotsRemaining > 0 then
        if burstCooldownTimer > 0 then
            burstCooldownTimer = burstCooldownTimer - dt
        else
            if currentAmmo > 0 then
                fireBullet()
                burstShotsRemaining = burstShotsRemaining - 1
                if burstShotsRemaining > 0 then
                    burstCooldownTimer = w.burstDelay or 0.0
                else
                    canShoot = true
                    shootCooldown = (w.burstDelay or 0.0) * 2.0
                end
            else
                burstShotsRemaining = 0
                canShoot = true
                spawn(reloadTask, "autoReloadTask")
            end
        end
        return
    end

    -- Single / Automatic
    local canFire = false
    if w.fireType == "single" then
        canFire = lmbDown and not lmbWasDown
    else
        canFire = lmbDown
    end
    lmbWasDown = lmbDown

    if canFire and canShoot and shootCooldown <= 0 and not isReloading then
        if currentAmmo > 0 then
            fireBullet()
            if (w.burstAmount or 1) > 1 then
                burstShotsRemaining = (w.burstAmount or 1) - 1
                burstCooldownTimer = w.burstDelay or 0.0
                canShoot = false
            else
                canShoot = true
                shootCooldown = w.fireRate or 0.1
            end
        elseif not isReloading then
            spawn(reloadTask, "autoReloadTask")
        end
    end

	-- ==========================================
    -- УМНОЕ УПРАВЛЕНИЕ АНИМАЦИЯМИ ВЬЮМОДЕЛИ
    -- ==========================================
	if viewmodelAnim and not isReloading then
		local shouldPlayIdle = false

		-- Проверяем, играется ли сейчас клип "shot"
		local isShotPlaying = viewmodelAnim.isPlayingAnim and viewmodelAnim.currentClip == "shot"

		if not isShotPlaying then
			-- Если shot не играется, решаем, нужно ли переключиться на idle
			if not lmbDown then
				-- Кнопка отпущена → можно на idle
				shouldPlayIdle = true
			else
				-- Кнопка зажата, но для single-оружия переключаем на idle только если не можем стрелять
				if w.fireType == "single" and not (canFire and shootCooldown <= 0 and not isReloading) then
					shouldPlayIdle = true
				end
			end
		end
		-- Если shot всё ещё играется, ничего не делаем – анимация продолжается

		if shouldPlayIdle then
			pcall(function() viewmodelAnim:playClip("idle") end)
		end
	end
end

function stopMinigunSound(w)
    if isMinigunLooping then
        Network.send(string.format("MNG_STOP %d", myID))
    end
    isMinigunLooping = false
    if w.fireSoundType == "minigun" and minigunLoopSoundId ~= 0 then
        Sound.stop(minigunLoopSoundId)
        minigunLoopSoundId = 0
    end
    if w.stopSound ~= "" then
        Sound.play2D(w.stopSound, 0.5, 1.0, false)
    end
end

function fireBullet()
    local w = WEAPONS[currentWeapon]
    currentAmmo = currentAmmo - 1
    updateHUD()
	
	myChar.flash.intensity = 200
	spawn(function()
		wait(0.05)
		myChar.flash.intensity = 0
	end)

    if w.sound ~= "" and w.fireType ~= "minigun" then
        Sound.play2D(w.sound, 0.5, math.randf(0.9, 1.1), false)
    end

    if w.isProjectile then
        throwProjectile(w)
        return
    end

    -- ==========================================
    -- РАСЧЕТ ДИНАМИЧЕСКОГО СПРЕДА
    -- ==========================================
    local actualSpread = (w.baseSpread or 0.0) + currentSpreadMult

    local camPos = Camera.getPosition()

    local netYawRad = math.rad(camYaw + (math.random() - 0.5) * actualSpread)
    local netPitchRad = math.rad(camPitch + (math.random() - 0.5) * actualSpread)
    local netShootDir = vec3(
        math.cos(netYawRad) * math.cos(netPitchRad),
        math.sin(netYawRad) * math.cos(netPitchRad),
        math.sin(netPitchRad)
    )

    Network.send(string.format("SHOOT %d %.2f %.2f %.2f %.2f %.2f %.2f",
        myID, camPos.x, camPos.y, camPos.z,
        netShootDir.x, netShootDir.y, netShootDir.z))
		
	if w.doTracer then createFire(camPos + netShootDir * 1.2) end

    local localTracerDist = shootRange
    if netTracerHit and netTracerHit.hit then
        localTracerDist = netTracerHit.distance
    end
    if w.doTracer and (w.bulletsToFire == 1 or math.random() < 0.15) then
        -- Рисуем длинный трассер ТОЛЬКО для мгновенных пуль (снайперки, нож).
        -- Для летящих пуль трассер-точка уже спавнится внутри simulateBulletFlight.
        if w.instantBullet == true then
            createTracer(camPos, netShootDir, localTracerDist)
        end
    end

    for bulletIdx = 1, w.bulletsToFire do
        -- Для каждой отдельной пули в очереди тоже применяем динамический спред
        local spreadYawRad = math.rad(camYaw + (math.random() - 0.5) * actualSpread)
        local spreadPitchRad = math.rad(camPitch + (math.random() - 0.5) * actualSpread)
        local shootDir = vec3(
            math.cos(spreadYawRad) * math.cos(spreadPitchRad),
            math.sin(spreadYawRad) * math.cos(spreadPitchRad),
            math.sin(spreadPitchRad)
        )

		local lookDir = vec3(
			math.cos(math.rad(camYaw)) * math.cos(math.rad(camPitch)),
			math.sin(math.rad(camYaw)) * math.cos(math.rad(camPitch)),
			math.sin(math.rad(camPitch))
		)
		local rayPos = camPos + lookDir * 0.5
		local rayDistance = shootRange - 0.5 -- Чуть короче, чтобы не улетала за границу

        local penetrationsLeft = (w.ammoType == "penetrative") and (w.ammoParameter or 1) or 0
        local isExplosive = (w.ammoType == "explosive")
        local finalExplosionPos = nil

        -- Запускаем симуляцию полета пули
        spawnBullet(shootDir, rayPos, rayDistance, penetrationsLeft, isExplosive, w, w.instantBullet == true, false, myID, myTeam)
        --[[if w.instantBullet == true then
            -- Мгновенное попадание (Hitscan)
            simulateBulletFlight(shootDir, rayPos, rayDistance, penetrationsLeft, isExplosive, w, true)
        else
            -- Летящая пуля (Выполняем в отдельной корутине, чтобы не фризить игру)
            spawn(function()
                simulateBulletFlight(shootDir, rayPos, rayDistance, penetrationsLeft, isExplosive, w, false)
            end, "flying_bullet_" .. bulletIdx)
			spawnBullet(shootDir, rayPos, rayDistance, penetrationsLeft, isExplosive, w, w.instantBullet == true, false, myID, myTeam)
        end]]
    end

    -- ==========================================
    -- ДИНАМИЧЕСКАЯ ОТДАЧА
    -- ==========================================
    local actualRecoil = recoilKick * (1.0 + currentRecoilMult)
    
    camRecoilQueue = camRecoilQueue + actualRecoil
    camRecoilRecovery = camRecoilRecovery + (actualRecoil / 3.0)
    
    vmRecoilPos = vmRecoilPos + vmKickBack * (1.0 + currentRecoilMult * 0.5)
    vmRecoilRot = vmRecoilRot + vmKickUp * (1.0 + currentRecoilMult * 0.5)
    
    -- ==========================================
    -- НАРАСТАНИЕ ШТРАФОВ ПОСЛЕ ВЫСТРЕЛА
    -- ==========================================
    currentSpreadMult = math.min(w.maxSpread or 5.0, currentSpreadMult + (w.spreadPerShot or 0.5))
    currentRecoilMult = math.min(w.maxRecoilMult or 2.0, currentRecoilMult + (w.recoilPerShot or 0.3))

    -- Анимация выстрела вьюмодели
    if viewmodelAnim then
        pcall(function() viewmodelAnim:playClip("shot") end)
    end
end

-- ==========================================
-- ДВИЖЕНИЕ
-- ==========================================
function groundCheckTask()
    while true do
        if netMode == "none" or not myChar or not myChar.physRoot then
            return
        end

        local ppos = Physics.getPosition(myChar.physRoot.name)

        local rayStart = ppos - vec3(0, 0, PLAYER_HALF_H - 0.1)
        local rayEnd = ppos - vec3(0, 0, PLAYER_HALF_H + 0.5)

        local hit = Physics.raycast(rayStart, rayEnd)

        if hit and hit.hit then
            isGrounded = true
        else
            isGrounded = false
        end

        wait(0.05)
    end
end

function processMovement(dt)
    if isPaused then return end
    local dx, dy = Input.getMouseDelta(); if math.abs(dx) > 100 or math.abs(dy) > 100 then dx, dy = 0, 0 end
    local invertX = invertMouseX and -1 or 1; local invertY = invertMouseY and -1 or 1

    -- ==========================================
    -- ПРИЦЕЛИВАНИЕ (ADS)
    -- ==========================================
    local rmbDown = Input.isMouseButtonDown(Input.Mouse.Right)
    local w = WEAPONS[currentWeapon]
    local weaponAimFov = (w and w.aimedFov) or 50.0 -- Фоллбэк на 50, если нет в таблице

    if rmbDown and not isAiming then
        isAiming = true
        targetAimFov = weaponAimFov
    elseif not rmbDown and isAiming then
        isAiming = false
        targetAimFov = defaultFov
    end

    -- Плавная интерполяция FOV (занимает примерно 0.5 секунды)
    -- Множитель 4.5 дает идеальный ease-out эффект
    currentAimFov = currentAimFov + (targetAimFov - currentAimFov) * math.min(1.0, dt * 4.5)
    
    -- Прогресс прицеливания от 0.0 (не целится) до 1.0 (полностью прицелился)
    local aimProgress = 0.0
    if math.abs(defaultFov - weaponAimFov) > 0.1 then
        aimProgress = 1.0 - ((currentAimFov - weaponAimFov) / (defaultFov - weaponAimFov))
        aimProgress = math.clamp(aimProgress, 0.0, 1.0)
    end

    local recoilDelta = 0.0
    if camRecoilQueue > 0 then
        recoilDelta = math.min(camRecoilQueue, dt * 80.0) 
        camRecoilQueue = camRecoilQueue - recoilDelta
    elseif camRecoilRecovery > 0 then
        recoilDelta = -math.min(camRecoilRecovery, dt * 70.0) 
        camRecoilRecovery = camRecoilRecovery - math.abs(recoilDelta)
    end
    
    -- ЖЕЛЕЗОБЕТОННЫЙ ПРИЦЕЛ (для стрельбы)
    -- 🔥 Фишка: при прицеливании чувствительность плавно падает на 25%
    local adsSensMult = 1.0 - (aimProgress * 0.25)
    
    camYaw = camYaw + (dx * mouseSens * invertX * adsSensMult)
    camPitch = camPitch - (dy * mouseSens * invertY * adsSensMult) + recoilDelta
    camPitch = math.clamp(camPitch, -89.0, 89.0)

    local yawRad = math.rad(camYaw)
    local hFwdX, hFwdY = math.cos(yawRad), math.sin(yawRad)
    local hRightX, hRightY = -math.sin(yawRad), math.cos(yawRad)

    local moveFwd, moveRight = 0.0, 0.0
    if Input.isKeyDown(Input.Keys.W) then moveFwd = moveFwd + 1.0 end
    if Input.isKeyDown(Input.Keys.S) then moveFwd = moveFwd - 1.0 end
    if Input.isKeyDown(Input.Keys.A) then moveRight = moveRight + 1.0 end
    if Input.isKeyDown(Input.Keys.D) then moveRight = moveRight - 1.0 end

    local vel = Physics.getVelocity(myChar.physRoot.name)
    local speedSq = vel.x * vel.x + vel.y * vel.y + vel.z * vel.z
    if speedSq > MAX_VELOCITY * MAX_VELOCITY then
        local currentSpeed = math.sqrt(speedSq)
        local scale = MAX_VELOCITY / currentSpeed
        vel = vec3(vel.x * scale, vel.y * scale, vel.z * scale)
        Physics.setVelocity(myChar.physRoot.name, vel)
    end
    if vel.z > jumpForce * 2.0 then
        vel.z = jumpForce * 2.0
        Physics.setVelocity(myChar.physRoot.name, vel)
    end

    local speed = Input.isKeyDown(Input.Keys.Shift) and sprintSpeed or walkSpeed
    if not isGrounded then speed = speed * airControl end

    if moveFwd ~= 0 or moveRight ~= 0 then
        local len = math.sqrt(moveFwd * moveFwd + moveRight * moveRight)
        moveFwd, moveRight = moveFwd / len, moveRight / len
        Physics.setVelocity(myChar.physRoot.name, vec3(
            (hFwdX * moveFwd + hRightX * moveRight) * speed,
            (hFwdY * moveFwd + hRightY * moveRight) * speed,
            vel.z
        ))
    elseif isGrounded then
        Physics.setVelocity(myChar.physRoot.name, vec3(0, 0, vel.z))
    end

    Physics.setAngularVelocity(myChar.physRoot.name, vec3(0, 0, 0))
    Physics.activate(myChar.physRoot.name)

    local spaceDown = Input.isKeyDown(Input.Keys.Space)
    if spaceDown and isGrounded and not wantJump then
        wantJump = true
        local v = Physics.getVelocity(myChar.physRoot.name)
        Physics.setVelocity(myChar.physRoot.name, vec3(v.x, v.y, jumpForce))
        isGrounded = false
    elseif not spaceDown then
        wantJump = false
    end

    local ppos = Physics.getPosition(myChar.physRoot.name)
    local eyePos = ppos + vec3(0, 0, eyeOffset) + vec3(hFwdX, hFwdY, 0) * camFwdOffset
    
    -- ==========================================
    -- ОБЩИЕ РАСЧЕТЫ ДЛЯ КАМЕРЫ И ВЬЮМОДЕЛИ
    -- ==========================================
    local hSpeed = math.sqrt(vel.x * vel.x + vel.y * vel.y)
    local isMoving = (moveFwd ~= 0 or moveRight ~= 0) and isGrounded

    -- Фаза покачивания (синхронизирует камеру и оружие)
    if isMoving then
        local freq = (Input.isKeyDown(Input.Keys.Shift) and 11.0 or 8.0)
        bobPhase = bobPhase + dt * freq
    else
        bobPhase = bobPhase * (1.0 - math.min(1, dt * 5.0))
    end

    -- ==========================================
    -- CINEMATIC CAMERA SWAY (Из Роблокса)
    -- ==========================================
    local targetVisYaw = 0.0
    local targetVisPitch = 0.0
    camIdleTimer = camIdleTimer + dt

    if isMoving then
        -- Покачивание камеры при ходьбе (зависит от скорости)
        local intensity = math.min(hSpeed / 15.0, 1.0) 
		targetVisYaw = math.cos(bobPhase * 0.5) * 0.6 * intensity
        targetVisPitch = math.sin(bobPhase) * 0.45 * intensity
    else
        -- Микро-движение камеры когда стоишь (имитация дыхания/жизни)
        -- Используем разные частоты для X и Y чтобы не было механичности
        targetVisYaw = math.cos(camIdleTimer * 0.5) * 0.05 + math.cos(camIdleTimer * 0.7) * 0.02
        targetVisPitch = math.sin(camIdleTimer * 0.4) * 0.04 + math.sin(camIdleTimer * 0.9) * 0.01
    end

    -- Плавная интерполяция визуальных свеев
    camVisYaw = camVisYaw + (targetVisYaw - camVisYaw) * math.min(1, dt * 8.0)
    camVisPitch = camVisPitch + (targetVisPitch - camVisPitch) * math.min(1, dt * 8.0)

    -- ==========================================
    -- ИТОГОВАЯ КАМЕРА
    -- ==========================================
    -- Добавляем визуальный sway к чистому прицелу
    local finalVisYaw = camYaw + camVisYaw
    local finalVisPitch = camPitch + camVisPitch
    
    local visYawRad = math.rad(finalVisYaw)
    local visPitchRad = math.rad(finalVisPitch)

    local lookDir = vec3(
        math.cos(visYawRad) * math.cos(visPitchRad),
        math.sin(visYawRad) * math.cos(visPitchRad),
        math.sin(visPitchRad)
    )
    Camera.setPosition(eyePos)
    Camera.lookAt(eyePos + lookDir)
    
    -- 🔥 ПРИМЕНЯЕМ ЗУМ КАМЕРЫ (если не активна катсцена убийства)
    if not centerImageActive then
        Camera.setFOV(currentAimFov)
    end

    -- Анимация персонажа
    local currentPos = Physics.getPosition(myChar.physRoot.name)
    myChar.anim.position = currentPos
    myChar.anim.rotation = vec3(0, 0, camYaw) --addass
    myChar.anim.scale = vec3(R6_SCALE, R6_SCALE, R6_SCALE)
    updateLocalAnimation()

    if isGrounded then
        local speed = math.sqrt(vel.x * vel.x + vel.y * vel.y)
        if Input.isKeyDown(Input.Keys.Shift) and speed > 1.0 then
            local stepDelay = getStepDelay(speed)
            footstepTimer = footstepTimer + dt
            if footstepTimer >= stepDelay then
                footstepTimer = 0.0
				Sound.play2D("footstepConcrete", 0.5, math.randf(0.85, 1.15), false)
            end
        else
            footstepTimer = 0.6
        end
    end

    -- ==========================================
    -- VIEWMODEL С СУПЕР-КРЕНАМИ (только если есть вьюмодель)
    -- ==========================================
    if viewmodelAnim then
        vmRecoilPos = vmRecoilPos + (0 - vmRecoilPos) * math.min(1, dt * recoilRecovery)
        vmRecoilRot = vmRecoilRot + (0 - vmRecoilRot) * math.min(1, dt * recoilRecovery)
        
        local Fx, Fy, Fz = lookDir.x, lookDir.y, lookDir.z
        local Rx, Ry, Rz = -Fy, Fx, 0
        local Rlen = math.sqrt(Rx * Rx + Ry * Ry)
        if Rlen > 0.001 then Rx = Rx / Rlen; Ry = Ry / Rlen
        else Rx = -math.sin(yawRad); Ry = math.cos(yawRad) end
        
        local Ux, Uy, Uz = Fy * Rz - Fz * Ry, Fz * Rx - Fx * Rz, Fx * Ry - Fy * Rx
        local Ulen = math.sqrt(Ux * Ux + Uy * Uy + Uz * Uz)
        if Ulen > 0.001 then Ux = Ux / Ulen; Uy = Uy / Ulen; Uz = Uz / Ulen end
        
        -- 1. Bob
        local targetBobX = isMoving and (math.sin(bobPhase) * 0.012) or 0.0
        local targetBobY = isMoving and (math.abs(math.cos(bobPhase)) * 0.015) or 0.0
        bobAmountX = bobAmountX + (targetBobX - bobAmountX) * math.min(1, dt * 10.0)
        bobAmountY = bobAmountY + (targetBobY - bobAmountY) * math.min(1, dt * 10.0)

        -- 2. Mouse Sway
        local targetSwayX = -dx * 0.003
        local targetSwayY = -dy * 0.003
        vmMouseSwayX = vmMouseSwayX + (targetSwayX - vmMouseSwayX) * math.min(1, dt * 15.0)
        vmMouseSwayY = vmMouseSwayY + (targetSwayY - vmMouseSwayY) * math.min(1, dt * 15.0)

        -- 3. Tilt
        local localVelX = (vel.x * Rx + vel.y * Ry)
        local targetTilt = -localVelX * 1.2
        vmTiltZ = vmTiltZ + (targetTilt - vmTiltZ) * math.min(1, dt * 6.0)
        
        local targetMouseRoll = math.clamp(-dx * 0.15, -2.5, 2.5)
        vmMouseRoll = vmMouseRoll + (targetMouseRoll - vmMouseRoll) * math.min(1, dt * 12.0)

        -- 4. Jump
        local targetJumpZ = isGrounded and 0.0 or (-vel.z * 0.025)
        vmJumpZ = vmJumpZ + (targetJumpZ - vmJumpZ) * math.min(1, dt * 5.0)

		-- 5. Итоговая позиция с учётом прицеливания
		local fwdOff = 0.6 - vmRecoilPos * 0.2
		
		-- 🔥 СДВИГ К ЦЕНТРУ ПРИ ПРИЦЕЛИВАНИИ
		local baseRightOff = 0.3
		local baseUpOff = -0.25
		local aimRightOff = 0.0   -- Строго по центру X
		local aimUpOff = -0.12    -- Чуть выше по центру Y
		
		-- Интерполируем между базовым положением и центром
		local finalRightOff = baseRightOff + (aimRightOff - baseRightOff) * aimProgress
		local finalUpOff = baseUpOff + (aimUpOff - baseUpOff) * aimProgress
		
		local rightOff = finalRightOff + bobAmountX + vmMouseSwayX
		local upOff = finalUpOff + vmRecoilPos * 0.1 + bobAmountY + vmMouseSwayY + vmJumpZ
		
		local finalPos = vec3(
			eyePos.x + Fx * fwdOff + Rx * rightOff + Ux * upOff,
			eyePos.y + Fy * fwdOff + Ry * rightOff + Uy * upOff,
			eyePos.z + Fz * fwdOff + Rz * rightOff + Uz * upOff
		)

        local totalRoll = vmTiltZ + vmMouseRoll
        local finalRot = vec3(0, camPitch + vmRecoilRot * 0.2, camYaw + 180 + totalRoll)

        viewmodelAnim.position = finalPos
        viewmodelAnim.rotation = finalRot
    end
end

-- ==========================================
-- ИНИЦИАЛИЗАЦИЯ И ОСТАНОВКА
-- ==========================================
function onStart()
    clearScene()
    Sun.setIntensity(4.0)
    Sun.setDirection(vec3(-0.2, -0.3, -0.8))
    Camera.setInputMode(InputMode.None)
    showConnectScreen()
end

function selectSkin(skin, btnId)
    mySkin = skin
    for id, el in pairs(skinBtns) do
        RmlUi.setProperty(el, "background", id == btnId and "#ffaa00" or "#333333")
    end
end

function selectMusicKit(kit, btnId)
    myMusicKit = kit
    for id, el in pairs(musicBtns) do
        RmlUi.setProperty(el, "background", id == btnId and "#ffaa00" or "#333333")
    end
end

menuCamBasePos = vec3(0, 0, 0)
menuCamBaseFront = vec3(1, 0, 0)
menuCamActive = false

function showConnectScreen()
	loadMap("backrooms", "_bg")
	local bgCam = Object.find("BackgroundCam")
	    if bgCam then
        menuCamBasePos = bgCam.position
        menuCamBaseFront = bgCam.rotation
        Camera.setPosition(menuCamBasePos)
        Camera.setFront(menuCamBaseFront)
		Camera.setFOV(bgCam.fov)
		Graphics.saturation = bgCam.cameraSaturation
		Graphics.brightness = bgCam.cameraBrightness
		Graphics.chromaticAberrationStrength = bgCam.cameraChromaticAberration
    end
    menuCamActive = true

	ambSnd = Sound.play2D("SpencerBaggett-HazardousEnvironments", 1.0, 1.0, false)

    connectDoc = RmlUi.loadDocument("scripts/connect_screen.rml")
    if connectDoc then
        RmlUi.show(connectDoc, 2, 1)
        ipInputEl = RmlUi.getElementById(connectDoc, "ip-input")
        nameInputEl = RmlUi.getElementById(connectDoc, "name-input")
        local hostBtn = RmlUi.getElementById(connectDoc, "btn-host")
        local joinBtn = RmlUi.getElementById(connectDoc, "btn-join")
		local creditsBtn = RmlUi.getElementById(connectDoc, "btn-credits")
		local closeCreditsBtn = RmlUi.getElementById(connectDoc, "btn-close-credits")
		local creditsWindow = RmlUi.getElementById(connectDoc, "credits-window")
        RmlUi.addEventListener(hostBtn, "click", function()
			Sound.stopAll()
			Camera.setFOV(65)
			Graphics.saturation = 1.3
			Graphics.brightness = flashOrigBrightness
			Graphics.chromaticAberrationStrength = caCurrent
            hostGame()
        end)
        RmlUi.addEventListener(joinBtn, "click", function()
			Sound.stopAll()
			Camera.setFOV(65)
			Graphics.saturation = 1.3
			Graphics.brightness = flashOrigBrightness
			Graphics.chromaticAberrationStrength = caCurrent
            joinGame()
        end)
		if creditsBtn and creditsWindow then
			RmlUi.addEventListener(creditsBtn, "click", function()
				RmlUi.setProperty(creditsWindow, "display", "flex") -- Показываем окно
			end)
		end
		
		if closeCreditsBtn and creditsWindow then
			RmlUi.addEventListener(closeCreditsBtn, "click", function()
				RmlUi.setProperty(creditsWindow, "display", "none") -- Скрываем окно
			end)
		end

        skinBtns = {}
        local s1 = RmlUi.getElementById(connectDoc, "btn-skin1")
        local s2 = RmlUi.getElementById(connectDoc, "btn-skin2")
        local s3 = RmlUi.getElementById(connectDoc, "btn-skin3")
        if s1 then
            skinBtns[1] = s1
            RmlUi.addEventListener(s1, "click", function()
                selectSkin("navalniySkin1", 1)
            end)
        end
		if s2 then
            skinBtns[2] = s2
            RmlUi.addEventListener(s2, "click", function()
                selectSkin("jennySkin1", 2)
            end)
        end
        if s3 then
            skinBtns[3] = s3
            RmlUi.addEventListener(s3, "click", function()
                selectSkin("adolfSkin1", 3)
            end)
        end
        selectSkin(mySkin, 1)

        musicBtns = {}
        local m1 = RmlUi.getElementById(connectDoc, "btn-music1")
        local m2 = RmlUi.getElementById(connectDoc, "btn-music2")
        local m3 = RmlUi.getElementById(connectDoc, "btn-music3")
        if m1 then
            musicBtns[1] = m1
            RmlUi.addEventListener(m1, "click", function()
                selectMusicKit("muskit1_mvp", 1)
            end)
        end
        if m2 then
            musicBtns[2] = m2
            RmlUi.addEventListener(m2, "click", function()
                selectMusicKit("muskit2_mvp", 2)
            end)
        end
        if m3 then
            musicBtns[3] = m3
            RmlUi.addEventListener(m3, "click", function()
                selectMusicKit("muskit3_mvp", 3)
            end)
        end
        selectMusicKit(myMusicKit, 1)

        cfgStartMoneyEl = RmlUi.getElementById(connectDoc, "cfg-start-money")
        cfgMaxMoneyEl = RmlUi.getElementById(connectDoc, "cfg-max-money")
        cfgBuyTimeEl = RmlUi.getElementById(connectDoc, "cfg-buy-time")
        cfgCostMultEl = RmlUi.getElementById(connectDoc, "cfg-cost-mult")
        cfgRestoreDefEl = RmlUi.getElementById(connectDoc, "cfg-restore-default")
        cfgRestoreMonEl = RmlUi.getElementById(connectDoc, "cfg-restore-money")
        cfgMoneyWinEl = RmlUi.getElementById(connectDoc, "cfg-money-win")
        cfgMoneyLoseEl = RmlUi.getElementById(connectDoc, "cfg-money-lose")
        cfgMoneyKillEl = RmlUi.getElementById(connectDoc, "cfg-money-kill")
        cfgMoneyDeathEl = RmlUi.getElementById(connectDoc, "cfg-money-death")
        cfgRemoveWeaponsDeathEl = RmlUi.getElementById(connectDoc, "cfg-remove-weapons-death")
        cfgRoundsToWinEl = RmlUi.getElementById(connectDoc, "cfg-rounds-to-win")
        cfgTieRoundEl = RmlUi.getElementById(connectDoc, "cfg-tie-round")
        cfgRemoveWeaponsDeathEl = RmlUi.getElementById(connectDoc, "cfg-remove-weapons-death")
        cfgRoundsToWinEl = RmlUi.getElementById(connectDoc, "cfg-rounds-to-win")
        cfgTieRoundEl = RmlUi.getElementById(connectDoc, "cfg-tie-round")
        cfgGameModeEl = RmlUi.getElementById(connectDoc, "cfg-game-mode")
		cfgMapEl = RmlUi.getElementById(connectDoc, "cfg-map")
		if cfgMapEl then
			RmlUi.setAttribute(cfgMapEl, "value", currentMap)
		end
    end
end

function showTeamSelect()
    teamSelectDoc = RmlUi.loadDocument("scripts/team_select.rml")
    if teamSelectDoc then
        RmlUi.show(teamSelectDoc, 2, 1)
        local redBtn = RmlUi.getElementById(teamSelectDoc, "btn-red")
        local blueBtn = RmlUi.getElementById(teamSelectDoc, "btn-blue")
        RmlUi.addEventListener(redBtn, "click", function()
            selectTeam(1)
        end)
        RmlUi.addEventListener(blueBtn, "click", function()
            selectTeam(2)
        end)
    end
end

function onStop()
    if netMode == "host" and Network.isConnected() then
        Network.send("END")
    end

	if netMode == "client" and Network.isConnected() and myID ~= 0 then
		Network.send(string.format("DISCONNECT %d", myID))
	end

    Camera.setInputMode(InputMode.Editor)

    if hudDoc and RmlUi then
        RmlUi.destroy(hudDoc)
    end

    if connectDoc and RmlUi then
        RmlUi.destroy(connectDoc)
    end

    if teamSelectDoc and RmlUi then
        RmlUi.destroy(teamSelectDoc)
    end

    if buyMenuDoc and RmlUi then
        RmlUi.destroy(buyMenuDoc)
    end

    if pauseDoc and RmlUi then
        RmlUi.destroy(pauseDoc)
        pauseDoc = nil
    end

    if Network.isConnected() then
        Network.disconnect()
    end
end

Engine.start()