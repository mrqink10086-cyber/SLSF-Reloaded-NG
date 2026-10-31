#pragma once
// SPDX-License-Identifier: Apache-2.0
// Derived interface declaration; see licenses/DeviousDevicesNG-NOTICE.txt.
#include <map>
#include <vector>
#include <string>
namespace DeviousDevicesAPI {
struct DeviceUnitPrototype;
enum BondageState : uint32_t {};
class DeviousDevicesAPI {
public:
virtual size_t GetVersion() const;
virtual const std::map<RE::TESObjectARMO*, DeviceUnitPrototype>& GetDatabase() const;
virtual RE::TESObjectARMO*  GetDeviceRender(RE::TESObjectARMO* a_invdevice) const;
virtual RE::TESObjectARMO*  GetDeviceInventory(RE::TESObjectARMO* a_renddevice) const;
virtual RE::TESForm*        GetPropertyForm(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, RE::TESForm* a_defvalue, int a_mode) const;
virtual int                 GetPropertyInt(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, int a_defvalue, int a_mode) const;
virtual float               GetPropertyFloat(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, float a_defvalue, int a_mode) const;
virtual bool                GetPropertyBool(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, bool a_defvalue, int a_mode) const;
virtual std::string         GetPropertyString(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, std::string a_defvalue, int a_mode) const;
virtual std::vector<RE::TESForm*>   GetPropertyFormArray(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, int a_mode) const;
virtual std::vector<int>            GetPropertyIntArray(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, int a_mode) const;
virtual std::vector<float>          GetPropertyFloatArray(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, int a_mode) const;
virtual std::vector<bool>           GetPropertyBoolArray(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, int a_mode) const;
virtual std::vector<std::string>    GetPropertyStringArray(RE::TESObjectARMO* a_invdevice, std::string a_propertyname, int a_mode) const;
virtual bool ApplyExpression(RE::Actor* a_actor, const std::vector<float> a_expression, float a_strength, bool a_openMouth,int a_priority) const;
virtual bool ResetExpression(RE::Actor* a_actor, int a_priority) const;
virtual void UpdateGagExpression(RE::Actor* a_actor) const;
virtual void ResetGagExpression(RE::Actor* a_actor) const;
virtual bool IsGagged(RE::Actor* a_actor) const;
virtual bool RegisterGagType(RE::BGSKeyword* a_keyword, std::vector<RE::TESFaction*> a_factions, std::vector<int> a_defaults) const;
virtual bool RegisterDefaultGagType(std::vector<RE::TESFaction*> a_factions, std::vector<int> a_defaults) const;
virtual void SetActorStripped(RE::Actor* a_actor, bool a_stripped, int a_armorfilter, int a_devicefilter) const;
virtual bool IsActorStripped(RE::Actor* a_actor) const;
virtual bool IsValidForHide(RE::TESObjectARMO* a_armor) const;
virtual std::vector<RE::TESObjectARMO*> GetDevices(RE::Actor* a_actor, int a_mode, bool a_worn) const;
virtual RE::TESObjectARMO*              GetWornDevice(RE::Actor* a_actor, RE::BGSKeyword* a_kw, bool a_fuzzy) const;
virtual std::vector<RE::TESObjectARMO*> GetWornDevices(RE::Actor* a_actor) const;
virtual RE::TESObjectARMO*              GetHandRestrain(RE::Actor* a_actor) const;
virtual BondageState                    GetBondageState(RE::Actor* a_actor) const;
virtual bool                            IsDevice(RE::TESObjectARMO* a_obj) const;
virtual bool                            ActorHasBlockingGag(RE::Actor* a_actor, RE::TESObjectARMO* a_gag = nullptr) const;
};
}
