﻿namespace CitizenFX.Core
{
    #region Blip
    public enum BlipColor
    {
        White = 0,
        DarkRed = 1,
        DarkGreen = 2,
        Cyan = 3,
        Grey = 4,
        Yellow = 5,
        Orange = 6,
        Purple = 7,
        Green = 8,
        Red = 9,
        LightRed = 10,
        LightOrange = 11,
        DarkTurquoise = 12,
        Turquoise = 13,
        LightYellow = 14
    }

    public enum BlipDisplay
    {
        Hidden = 0,
        ArrowOnly = 1,
        MapOnly = 2,
        ArrowAndMap = 4,
    }

    public enum BlipIcon
    {
        Misc_Destination,
        Misc_Destination1,
        Misc_Destination2,
        Misc_Objective,
        Misc_Objective4,
        Misc_Objective5,
        Misc_Player,
        Misc_North,
        Misc_Waypoint,
        Weapon_Pistol,
        Weapon_Shotgun,
        Weapon_SMG,
        Weapon_Rifle,
        Weapon_Rocket,
        Weapon_Grenade,
        Weapon_Molotov,
        Weapon_Sniper,
        Weapon_BaseballBat,
        Weapon_Knife,
        Pickup_Health,
        Pickup_Armor,
        Building_BurgerShot,
        Building_CluckinBell,
        Person_Vlad,
        Building_Internet,
        Person_Manny,
        Person_LittleJacob,
        Person_Roman,
        Person_Faustin,
        Building_Safehouse,
        Misc_TaxiRank,
        Person_Bernie,
        Person_Brucie,
        Person_Unknown,
        Person_Dwayne,
        Person_Elizabeta,
        Person_Gambetti,
        Person_JimmyPegorino,
        Person_Derrick,
        Person_Francis,
        Person_Gerry,
        Person_Katie,
        Person_Packie,
        Person_PhilBell,
        Person_PlayboyX,
        Person_RayBoccino,
        Misc_8BALL,
        Activity_Bar,
        Activity_BoatTour,
        Activity_Bowling,
        Building_ClothShop,
        Activity_Club,
        Activity_Darts,
        Person_Dwayne_Red,
        Activity_Date,
        Person_PlayboyX_Red,
        Activity_HeliTour,
        Activity_Restaurant,
        Building_TrainStation,
        Building_WeaponShop,
        Building_PoliceStation,
        Building_FireStation,
        Building_Hospital,
        Person_Male,
        Person_Female,
        Misc_FinishLine,
        Activity_StripClub,
        Misc_ConsoleGame,
        Misc_CopCar,
        Person_Dimitri,
        Activity_ComedyClub,
        Activity_CabaretClub,
        Misc_Ransom,
        Misc_CopHeli,
        Person_Michelle,
        Building_PayNSpray,
        Person_Assassin,
        Misc_Revenge,
        Misc_Deal,
        Building_Garage,
        Person_Lawyer,
        Misc_Trophy,
        Misc_MultiplayerTutorial,
        Building_TrainStation3,
        Building_TrainStation8,
        Building_TrainStationA,
        Building_TrainStationB,
        Building_TrainStationC,
        Building_TrainStationE,
        Building_TrainStationJ,
        Building_TrainStationK,
        Building_CarWash,
        Person_UnitedLibertyPaper,
        Misc_Boss,
        Misc_Base
    }

    public enum BlipType
    {
        Vehicle = 1,
        Ped = 2,
        Object = 3,
        Coordinate = 4,
        Contact = 5,
        Pickup = 6,
        Unknown = 7,
        Pickup2 = 8,
    }
    #endregion

    #region Weapon
    public enum Weapons
    {
        None = -1,
        Unarmed = 0,
        Melee_BaseballBat = 1,
        Melee_PoolCue = 2,
        Melee_Knife = 3,
        Thrown_Grenade = 4,
        Thrown_Molotov = 5,
        Misc_Rocket = 6,
        Handgun_Glock = 7,
        Misc_Unused0 = 8,
        Handgun_DesertEagle = 9,
        Shotgun_Basic = 10,
        Shotgun_Baretta = 11,
        SMG_Uzi = 12,
        SMG_MP5 = 13,
        Rifle_AK47 = 14,
        Rifle_M4 = 15,
        SniperRifle_Basic = 16,
        SniperRifle_M40A1 = 17,
        Heavy_RocketLauncher = 18,

        Heavy_FlameThrower = 19,
        Heavy_Minigun = 20,
        Episodic_01 = 21,
        Episodic_02 = 22,
        Episodic_03 = 23,
        Episodic_04 = 24,
        Episodic_05 = 25,
        Episodic_06 = 26,
        Episodic_07 = 27,
        Episodic_08 = 28,
        Episodic_09 = 29,
        Episodic_10 = 30,
        Episodic_11 = 31,
        Episodic_12 = 32,
        Episodic_13 = 33,
        Episodic_14 = 34,
        Episodic_15 = 35,
        Episodic_16 = 36,
        Episodic_17 = 37,
        Episodic_18 = 38,
        Episodic_19 = 39,
        Episodic_20 = 40,
        Episodic_21 = 41,
        Episodic_22 = 42,
        Episodic_23 = 43,
        Episodic_24 = 44,
        Misc_Camera = 45,
        Misc_Object = 46,
        Misc_LastWeaponType = 47,
        Misc_Armor = 48,
        Misc_RammedByCar = 49,
        Misc_RunOverByCar = 50,
        Misc_Explosion = 51,
        Misc_UziDriveby = 52,
        Misc_Drowning = 53,
        Misc_Fall = 54,
        Misc_Unidentified = 55,
        Misc_AnyMelee = 56,
        Misc_AnyWeapon = 57,

        TBOGT_GrenadeLauncher = 21,
        TBOGT_Pistol44 = 29,
        TBOGT_ExplosiveShotgun = 30,
        TBOGT_NormalShotgun = 31,
        TBOGT_AssaultSMG = 32,
        TBOGT_GoldenSMG = 33,
        TBOGT_AdvancedMG = 34,
        TBOGT_AdvancedSniper = 35,
        TBOGT_StickyBomb = 36,
        TBOGT_Parachute = 41,

        TLAD_GrenadeLauncher = 21,
        TLAD_AssaultShotgun = 22,
        TLAD_Poolcue = 24,
        TLAD_SawedOffShotgun = 26,
        TLAD_Automatic9mm = 27,
        TLAD_PipeBomb = 28,
    }

    public enum WeaponSlot
    {
        Unarmed,
        Melee,
        Handgun,
        Shotgun,
        SMG,
        Rifle,
        Sniper,
        Heavy,
        Thrown,
        Special,
        Gift,
        Parachute,
        DetonatorUnknown
    }
    #endregion

    #region Ped
    public enum RelationshipGroup
    {
        Player,
        Civillian_Male,
        Civillian_Female,
        Cop,
        Gang_Albanian,
        Gang_Biker1,
        Gang_Biker2,
        Gang_Italian,
        Gang_Russian1,
        Gang_Russian2,
        Gang_Irish,
        Gang_Jamaican,
        Gang_AfricanAmerican,
        Gang_Korean,
        Gang_ChineseJapanese,
        Gang_PuertoRican,
        Dealer,
        Medic,
        Fireman,
        Criminal,
        Bum,
        Prostitute,
        Special,
        Mission_1,
        Mission_2,
        Mission_3,
        Mission_4,
        Mission_5,
        Mission_6,
        Mission_7,
        Mission_8,
        NetworkPlayer_01,
        NetworkPlayer_02,
        NetworkPlayer_03,
        NetworkPlayer_04,
        NetworkPlayer_05,
        NetworkPlayer_06,
        NetworkPlayer_07,
        NetworkPlayer_08,
        NetworkPlayer_09,
        NetworkPlayer_10,
        NetworkPlayer_11,
        NetworkPlayer_12,
        NetworkPlayer_13,
        NetworkPlayer_14,
        NetworkPlayer_15,
        NetworkPlayer_16,
        NetworkPlayer_17,
        NetworkPlayer_18,
        NetworkPlayer_19,
        NetworkPlayer_20,
        NetworkPlayer_21,
        NetworkPlayer_22,
        NetworkPlayer_23,
        NetworkPlayer_24,
        NetworkPlayer_25,
        NetworkPlayer_26,
        NetworkPlayer_27,
        NetworkPlayer_28,
        NetworkPlayer_29,
        NetworkPlayer_30,
        NetworkPlayer_31,
        NetworkPlayer_32,
        NetworkTeam_1,
        NetworkTeam_2,
        NetworkTeam_3,
        NetworkTeam_4,
        NetworkTeam_5,
        NetworkTeam_6,
        NetworkTeam_7,
        NetworkTeam_8,
    }

    public enum Gender
    {
        Male,
        Female
    }
    #endregion

    #region Vehicle
    public enum VehicleDoors
    {
		LeftFront,
		RightFront,
		LeftRear,
		RightRear,
		Hood,
		Trunk,
	}

    public enum VehicleDoorLock
    {
        None = 0,
        CanOpenFromInside = 3,
        ImpossibleToOpen = 4,
    }

    public enum VehicleSeat
    {
        None = -3,
        AnyPassengerSeat = -2,
        Driver = -1,
        RightFront = 0,
        LeftRear = 1,
        RightRear = 2,
    }

    public enum VehicleWindow
    {
        LeftFront,
        RightFront,
        LeftRear,
        RightRear,
        Front,
        Rear
    }

    public enum VehicleWheel
    {
        /// <summary>
        /// Left Front Wheel. Front Wheel for bikes.
        /// </summary>
        FrontLeft = 0,
        /// <summary>
        /// Right Front Wheel. Unused for bikes.
        /// </summary>
        FrontRight = 1,
        /// <summary>
        /// Left Central Wheel. Unused for bikes and 4-wheeled vehicles.
        /// </summary>
        CenterLeft = 2,
        /// <summary>
        /// Right Central Wheel. Unused for bikes and 4-wheeled vehicles.
        /// </summary>
        CenterRight = 3,
        /// <summary>
        /// Left Rear Wheel. Rear Wheel for bikes.
        /// </summary>
        RearLeft = 4,
        /// <summary>
        /// Right Rear Wheel. Unused for bikes.
        /// </summary>
        RearRight = 5,
    }
    #endregion

    public enum Relationship
    {
        Hate = 5,
        Dislike = 4,
        Neutral = 3,
        Like = 2,
        Respect = 1,
        Companion = 0,
    }

    public enum Weather
    {
        ExtraSunny,
        Sunny,
        SunnyAndWindy,
        Cloudy,
        Raining,
        Drizzle,
        Foggy,
        ThunderStorm,
        ExtraSunny2,
        SunnyAndWindy2,
    }

    public enum ExplosionType
    {
        Default,
        Molotov,
        Rocket,
    }

    public enum GroundType
    {
        NextBelowCurrent,
        NextAboveCurrent,
        Closest,
        Lowest,
        Highest,
    }
}
