// GENERATED FILE - do not edit.
// Source: contracts/legacy-api.json (frozen, group 01).
// Regenerate with: python tools/gen-papyrus-bindings.py

static const detail::VmSignatureTableEntry kVmSignatures[] = {
    {
        "SLSF_Reloaded_CommentManager.NakedCommentPublicWhoreCheck",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_DataManager.FameCheck",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_DataManager.SetDefaults",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_DataManager.CheckSpreadableFame",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_DataManager.GetFameValue",
        false, std::int32_t,
        {{"LocationName", std::string}, {"FameCategory", std::string}},
    },
    {
        "SLSF_Reloaded_DataManager.SetFameValue",
        false, void,
        {{"LocationName", std::string}, {"FameCategory", std::string}, {"FameValue", std::int32_t}},
    },
    {
        "SLSF_Reloaded_DataManager.DecayDynamicAnonymityCheck",
        false, void,
        {{"LocationIndex", std::int32_t}, {"IsDefaultLocation", bool}},
    },
    {
        "SLSF_Reloaded_DataManager.GetExternalFlags",
        false, bool,
        {{"FlagName", std::string}},
    },
    {
        "SLSF_Reloaded_DataManager.RegisterExternalMod",
        false, void,
        {{"ModName", std::string}},
    },
    {
        "SLSF_Reloaded_DataManager.CountExternalMods",
        false, std::int32_t,
        {},
    },
    {
        "SLSF_Reloaded_DataManager.CleanExternalModList",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_DataManager.UnregisterExternalMod",
        false, void,
        {{"ModName", std::string}},
    },
    {
        "SLSF_Reloaded_DataManager.GetModFlagState",
        false, bool,
        {{"ModName", std::string}, {"FlagName", std::string}},
    },
    {
        "SLSF_Reloaded_DataManager.DoesFlagExist",
        false, std::string,
        {{"FlagName", std::string}},
    },
    {
        "SLSF_Reloaded_DataManager.IsModRegistered",
        false, bool,
        {{"ModName", std::string}},
    },
    {
        "SLSF_Reloaded_DataManager.SetExternalFlags",
        false, void,
        {{"ModName", std::string}, {"FlagName", std::string}, {"FlagValue", bool}},
    },
    {
        "SLSF_Reloaded_DataManager.CheckSexWorker",
        false, bool,
        {},
    },
    {
        "SLSF_Reloaded_DataManager.CheckFlags",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_DataManager.FameOverviewCheck",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_Data_Exporter.RunExport",
        false, void,
        {{"FileName", std::string}},
    },
    {
        "SLSF_Reloaded_Data_Exporter.ExportData",
        false, void,
        {{"FileName", std::string}},
    },
    {
        "SLSF_Reloaded_Data_Importer.RunImport",
        false, void,
        {{"FileName", std::string}},
    },
    {
        "SLSF_Reloaded_Data_Importer.ImportData",
        false, void,
        {{"FileName", std::string}},
    },
    {
        "SLSF_Reloaded_DynamicAnonymity.CompareLocations",
        false, void,
        {{"OldLocation", RE::BGSLocation*}, {"NewLocation", RE::BGSLocation*}},
    },
    {
        "SLSF_Reloaded_DynamicAnonymity.SetLastExitTime",
        false, void,
        {{"FameLocation", std::string}, {"GameTime", float}},
    },
    {
        "SLSF_Reloaded_DynamicAnonymity.SetLastEnterTime",
        false, void,
        {{"FameLocation", std::string}, {"GameTime", float}},
    },
    {
        "SLSF_Reloaded_DynamicAnonymity.CheckRecognitionTimeIncrease",
        false, void,
        {{"FameLocation", std::string}},
    },
    {
        "SLSF_Reloaded_DynamicAnonymity.CheckRecognitionTimeDecay",
        false, void,
        {{"FameLocation", std::string}},
    },
    {
        "SLSF_Reloaded_DynamicAnonymity.UpdateCurrentLocationAnonymity",
        false, void,
        {{"LocationIndex", std::int32_t}, {"GameTime", float}, {"IsDefaultLocation", bool}},
    },
    {
        "SLSF_Reloaded_DynamicAnonymity.GetAnonymity",
        false, void,
        {{"LocationRef", RE::BGSLocation*}},
    },
    {
        "SLSF_Reloaded_DynamicAnonymity.CheckAnonymityStatus",
        false, void,
        {{"LocationIndex", std::int32_t}, {"IsDefaultLocation", bool}},
    },
    {
        "SLSF_Reloaded_DynamicAnonymity.ThaneStatus",
        false, bool,
        {{"FameLocation", std::string}},
    },
    {
        "SLSF_Reloaded_FameManager.Startup",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_FameManager.UpdateFame",
        false, void,
        {{"UpdateMultiplier", std::int32_t}},
    },
    {
        "SLSF_Reloaded_FameManager.WipeCuckFame",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_FameManager.WipeUnfaithfulFame",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_FameManager.WipeAirheadFame",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_FameManager.WipeCumDumpFame",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_FameManager.CheckExternalFlags",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_FameManager.CanGainWhoreFame",
        false, bool,
        {},
    },
    {
        "SLSF_Reloaded_FameManager.CanGainSlutFame",
        false, bool,
        {{"FameLocation", std::string}},
    },
    {
        "SLSF_Reloaded_FameManager.CanGainExhibitionistFame",
        false, bool,
        {{"FameLocation", std::string}},
    },
    {
        "SLSF_Reloaded_FameManager.CanGainOralFame",
        false, bool,
        {{"FameLocation", std::string}},
    },
    {
        "SLSF_Reloaded_FameManager.CanGainAnalFame",
        false, bool,
        {{"FameLocation", std::string}},
    },
    {
        "SLSF_Reloaded_FameManager.CanGainNastyFame",
        false, bool,
        {{"FameLocation", std::string}},
    },
    {
        "SLSF_Reloaded_FameManager.CanGainPregnantFame",
        false, bool,
        {{"FameLocation", std::string}},
    },
    {
        "SLSF_Reloaded_FameManager.CanGainBoundFame",
        false, bool,
        {{"FameLocation", std::string}},
    },
    {
        "SLSF_Reloaded_FameManager.CanGainTattooFame",
        false, bool,
        {{"FameLocation", std::string}},
    },
    {
        "SLSF_Reloaded_FameManager.CanGainCumDumpFame",
        false, bool,
        {},
    },
    {
        "SLSF_Reloaded_FameManager.CheckTattooExtraFame",
        false, bool,
        {{"ExtraFame", std::string}},
    },
    {
        "SLSF_Reloaded_FameManager.FameGainRoll",
        false, void,
        {{"FameLocation", std::string}, {"CalledExternally", bool}, {"FameFromFriend", bool}, {"FameFromLover", bool}},
    },
    {
        "SLSF_Reloaded_FameManager.GainFame",
        false, void,
        {{"Category", std::string}, {"LocationName", std::string}, {"IsForeplay", bool}},
    },
    {
        "SLSF_Reloaded_FameManager.DecayFame",
        false, void,
        {{"DecayMultiplier", std::int32_t}},
    },
    {
        "SLSF_Reloaded_FameManager.SpreadFameRoll",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_FameManager.SpreadFame",
        false, void,
        {{"SpreadFromLocation", std::string}},
    },
    {
        "SLSF_Reloaded_FameManager.FameGainNotification",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_FameManager.FameDecayNotification",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_FameManager.CalculateFameSpread",
        false, std::int32_t,
        {{"FromFame", float}, {"TargetFame", float}},
    },
    {
        "SLSF_Reloaded_FameManager.FameSpreadNotification",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_FameManager.ClearFame",
        false, void,
        {{"LocationToClear", std::string}},
    },
    {
        "SLSF_Reloaded_FameManager.ClearAllFame",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_FameManager.UpdateGlobals",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_LegacyOverwrite.OverwriteLegacyFame",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_LocationManager.IsLocationValid",
        false, bool,
        {{"CheckedLocation", std::string}},
    },
    {
        "SLSF_Reloaded_LocationManager.LocationCanBeRegistered",
        false, bool,
        {{"LocationToRegister", std::string}, {"ExternalRegister", bool}},
    },
    {
        "SLSF_Reloaded_LocationManager.UpdateCustomLocationCount",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_LocationManager.FetchLocationName",
        false, std::string,
        {{"LocationRef", RE::BGSLocation*}},
    },
    {
        "SLSF_Reloaded_LocationManager.CurrentLocationName",
        false, std::string,
        {},
    },
    {
        "SLSF_Reloaded_LocationManager.GetLocalizedFameLocation",
        false, std::string,
        {{"LocationRef", RE::BGSLocation*}},
    },
    {
        "SLSF_Reloaded_LocationManager.GetFameLocation",
        false, std::string,
        {{"LocationRef", RE::BGSLocation*}},
    },
    {
        "SLSF_Reloaded_LocationManager.IsLocationExcluded",
        false, bool,
        {{"LocationRef", RE::BGSLocation*}},
    },
    {
        "SLSF_Reloaded_LocationManager.RegisterCustomLocation",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_LocationManager.AddToLocationList",
        false, void,
        {{"LocationRef", RE::BGSLocation*}},
    },
    {
        "SLSF_Reloaded_LocationManager.RemoveFromLocationList",
        false, void,
        {{"FameLocation", std::string}},
    },
    {
        "SLSF_Reloaded_LocationManager.UnregisterCustomLocation",
        false, void,
        {{"LocationIndexToUnregister", std::int32_t}},
    },
    {
        "SLSF_Reloaded_LocationManager.RegisterCustomLocationExternal",
        false, void,
        {{"LocationToRegister", std::string}, {"LocationRefToRegister", RE::BGSLocation*}},
    },
    {
        "SLSF_Reloaded_LocationManager.UnregisterCustomLocationExternal",
        false, void,
        {{"LocationToUnregister", std::string}},
    },
    {
        "SLSF_Reloaded_Logger.Log",
        true, void,
        {{"LogString", std::string}, {"Severity", std::int32_t}},
    },
    {
        "SLSF_Reloaded_Logger.EnableLogging",
        true, void,
        {{"EnableLogging", bool}},
    },
    {
        "SLSF_Reloaded_Logger.SetLogLevel",
        true, void,
        {{"LogLevel", std::int32_t}},
    },
    {
        "SLSF_Reloaded_MasterClock.Startup",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_MasterClock.HalfHourChecks",
        false, void,
        {{"Multiplier", std::int32_t}},
    },
    {
        "SLSF_Reloaded_MasterClock.DecayHandler",
        false, void,
        {{"Multiplier", std::int32_t}},
    },
    {
        "SLSF_Reloaded_MasterClock.SpreadHandler",
        false, void,
        {{"Multiplier", std::int32_t}},
    },
    {
        "SLSF_Reloaded_MasterClock.PeriodicFameUpdate",
        false, void,
        {{"Multiplier", std::int32_t}},
    },
    {
        "SLSF_Reloaded_ModEventListener.RegisterExternalEvents",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_ModIntegration.CheckInstalledMods",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_ModIntegration.GetFHUData",
        true, RE::TESForm*,
        {},
    },
    {
        "SLSF_Reloaded_ModIntegration.GetPWData",
        true, RE::TESForm*,
        {},
    },
    {
        "SLSF_Reloaded_ModIntegration.GetLegacyConfig",
        true, RE::TESForm*,
        {},
    },
    {
        "SLSF_Reloaded_ModIntegration.OverwriteLegacyConfig",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_ModIntegration.IsFMPregnant",
        false, bool,
        {{"actorRef", RE::Actor*}},
    },
    {
        "SLSF_Reloaded_ModIntegration.IsECPregnant",
        false, bool,
        {{"actorRef", RE::Actor*}},
    },
    {
        "SLSF_Reloaded_ModIntegration.IsESPregnant",
        false, bool,
        {{"actorRef", RE::Actor*}},
    },
    {
        "SLSF_Reloaded_ModIntegration.IsHentaiPregnant",
        false, bool,
        {{"actorRef", RE::Actor*}},
    },
    {
        "SLSF_Reloaded_ModIntegration.IsPublicWhore",
        false, bool,
        {},
    },
    {
        "SLSF_Reloaded_ModIntegration.GetFHUInflation",
        false, float,
        {{"actorRef", RE::Actor*}},
    },
    {
        "SLSF_Reloaded_PlayerScript.Startup",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_PlayerScript.RunNPCDetect",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_SexAnimationAnalyzer.AnimationAnalyze",
        false, void,
        {{"threadID", std::int32_t}},
    },
    {
        "SLSF_Reloaded_SexAnimationAnalyzer.AnimationSubAnalyze",
        false, void,
        {{"Actors", RE::BSScript::reference_array<RE::Actor*>}, {"PlayerController", RE::TESForm*}, {"PlayerThread", std::int32_t}, {"Foreplay", bool}, {"PlayerLocation", std::string}, {"Friend", bool}, {"Lover", bool}},
    },
    {
        "SLSF_Reloaded_Uninstall.RunUninstall",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_VisibilityManager.Startup",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_VisibilityManager.UpdateTattooSlots",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_VisibilityManager.FullUpdate",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_VisibilityManager.IsPlayerAnonymous",
        false, bool,
        {},
    },
    {
        "SLSF_Reloaded_VisibilityManager.CheckAppliedTattoos",
        false, void,
        {},
    },
    {
        "SLSF_Reloaded_VisibilityManager.CountAppliedTattoos",
        false, std::int32_t,
        {{"TattooArea", std::string}},
    },
    {
        "SLSF_Reloaded_VisibilityManager.CountVisibleTattoos",
        false, std::int32_t,
        {},
    },
    {
        "SLSF_Reloaded_VisibilityManager.CountBodyTattoos",
        false, std::int32_t,
        {},
    },
    {
        "SLSF_Reloaded_VisibilityManager.CountHandTattoos",
        false, std::int32_t,
        {},
    },
    {
        "SLSF_Reloaded_VisibilityManager.CountFaceTattoos",
        false, std::int32_t,
        {},
    },
    {
        "SLSF_Reloaded_VisibilityManager.CountFootTattoos",
        false, std::int32_t,
        {},
    },
    {
        "SLSF_Reloaded_VisibilityManager.IsBodyTattooVisible",
        false, bool,
        {{"SlotNumber", std::int32_t}},
    },
    {
        "SLSF_Reloaded_VisibilityManager.IsFaceTattooVisible",
        false, bool,
        {{"SlotNumber", std::int32_t}},
    },
    {
        "SLSF_Reloaded_VisibilityManager.IsHandTattooVisible",
        false, bool,
        {{"SlotNumber", std::int32_t}},
    },
    {
        "SLSF_Reloaded_VisibilityManager.IsFootTattooVisible",
        false, bool,
        {{"SlotNumber", std::int32_t}},
    },
    {
        "SLSF_Reloaded_VisibilityManager.IsAssCumVisible",
        false, bool,
        {},
    },
    {
        "SLSF_Reloaded_VisibilityManager.CountVisibleAssCum",
        false, std::int32_t,
        {},
    },
    {
        "SLSF_Reloaded_VisibilityManager.IsVaginalCumVisible",
        false, bool,
        {},
    },
    {
        "SLSF_Reloaded_VisibilityManager.CountVisibleVaginalCum",
        false, std::int32_t,
        {},
    },
    {
        "SLSF_Reloaded_VisibilityManager.IsOralCumVisible",
        false, bool,
        {},
    },
    {
        "SLSF_Reloaded_VisibilityManager.CountVisibleOralCum",
        false, std::int32_t,
        {},
    },
    {
        "SLSF_Reloaded_VisibilityManager.CheckBondage",
        false, void,
        {},
    },
};
