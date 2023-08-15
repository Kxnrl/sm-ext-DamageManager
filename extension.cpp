#include "extension.h"

#include <iplayerinfo.h>

#include "IBinTools.h"
#include "ISDKHooks.h"
#include "ISDKTools.h"

#include "takedamageinfohack.h"

DamageManager g_Damage;
SMEXT_LINK(&g_Damage);
extern sp_nativeinfo_t g_Natives[];

IGameConfig* g_pGameConf = nullptr;
ISDKHooks*   g_pSDKHooks = nullptr;
ISDKTools*   g_pSDKTools = nullptr;
IBinTools*   g_pBinTools = nullptr;
CGlobalVars* g_pGlobals  = nullptr;

enum hooks {
    HookTraceAttack,
    HookOnTakeDamage,
    HOOK_MAX_COUNT
};

enum filters {
    FilterDamageValid,
    FilterKillerValid,
    FilterWeaponValid,
    FilterDamageType,
    FilterSkipSelf,
    FilterSkipDead,
    FilterVictimTeam,
    FilterKillerTeam,
    FILTER_MAX_COUNT
};

class CPluginHook
{
public:
    IPlugin*         m_pPlugin;
    IPluginFunction* m_pFunction;
    cell_t           m_nFilter[FILTER_MAX_COUNT];

    CPluginHook(IPlugin* pPlugin, IPluginFunction* pFunction, const cell_t* filters) :
        m_pPlugin(pPlugin),
        m_pFunction(pFunction)
    {
        for (auto filter = 0; filter < FILTER_MAX_COUNT; filter++)
        {
            m_nFilter[filter] = filters[filter];
        }
    }
};

class CTrace
{
public:
    CTrace() :
        m_vecDirection(0, 0, 0), m_nTick(-1)
    {
        m_pTrace = new CGameTrace;
    }

    ~CTrace()
    {
        delete m_pTrace;
    }

    bool IsValid() const
    {
        return g_pGlobals->tickcount == m_nTick;
    }

    void Update(const CGameTrace* trace, const Vector& direction)
    {
        m_vecDirection.Init(direction.x, direction.y, direction.z);
        m_nTick = g_pGlobals->tickcount;
        memcpy(m_pTrace, trace, sizeof(CGameTrace));
    }

    Vector GetDirection() const
    {
        return m_vecDirection;
    }

    CGameTrace* GetGameTrace() const
    {
        return m_pTrace;
    }

private:
    Vector      m_vecDirection;
    CGameTrace* m_pTrace;
    cell_t      m_nTick;
};

int    g_iHookId[SM_MAXPLAYERS + 1][HOOK_MAX_COUNT];
volatile bool   g_bInHook;
volatile cell_t g_xObjectsPenetrated;
volatile cell_t g_xDamagedOtherPlayers;
volatile cell_t g_xDamageForce[3];
volatile cell_t g_xDamagePosition[3];

SH_DECL_MANUALHOOK3_void(TraceAttack, 0, 0, 0, CTakeDamageInfoHack&, const Vector&, CGameTrace*);
SH_DECL_MANUALHOOK1(OnTakeDamage, 0, 0, 0, int, CTakeDamageInfoHack&);

cell_t GetGameDataOffset(const char* name)
{
    if (g_pGameConf == nullptr)
        return -1;

    cell_t offset = -1;
    if (g_pGameConf->GetOffset(name, &offset))
        return offset;

    smutils->LogError(myself, "Failed to find '%s' offset", name);
    return -1;
}

cell_t GetSendPropOffset(const char* pClass, const char* pName)
{
    cell_t             offset = -1;
    sm_sendprop_info_t spi;
    if (gamehelpers->FindSendPropInfo(pClass, pName, &spi) && spi.prop != nullptr)
    {
        offset = spi.actual_offset;
    }

    return offset;
}

inline int GetEntityTeam(CBaseEntity* pEntity)
{
    static int offset = GetSendPropOffset("CBaseEntity", "m_iTeamNum");
    return *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(pEntity) + offset);
}

inline int GetClientHealth(CBaseEntity* pEntity)
{
    static int offset = GetSendPropOffset("CBasePlayer", "m_iHealth");
    return *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(pEntity) + offset);
}

void DamageManager::Hook_TraceAttack(CTakeDamageInfoHack& info, const Vector& direction, CGameTrace* trace) const
{
    if (m_pHooks.empty())
    {
        RETURN_META(MRES_IGNORED);
    }

    m_pLastTrace->Update(trace, direction);

    RETURN_META(MRES_IGNORED);
}

int DamageManager::Hook_OnTakeDamage(CTakeDamageInfoHack& info) const
{
    if (m_pHooks.empty())
    {
        RETURN_META_VALUE(MRES_IGNORED, 0);
    }

    const auto nRetVal = META_RESULT_ORIG_RET(int);

    auto         pVictim = META_IFACEPTR(CBaseEntity);
    CBaseEntity* pKiller = nullptr;

    const auto victim     = gamehelpers->EntityToBCompatRef(pVictim);
    const auto killer     = info.GetAttacker();
    const auto inflictor  = info.GetInflictor();
    const auto damage     = info.GetDamage();
    const auto damagetype = info.GetDamageType();
    const auto weapon     = info.GetWeapon();

    const auto force     = info.GetDamageForce();
    const auto pos       = info.GetDamagePosition();
    g_xDamageForce[0]    = sp_ftoc(force.x);
    g_xDamageForce[1]    = sp_ftoc(force.y);
    g_xDamageForce[2]    = sp_ftoc(force.z);
    g_xDamagePosition[0] = sp_ftoc(pos.x);
    g_xDamagePosition[1] = sp_ftoc(pos.y);
    g_xDamagePosition[2] = sp_ftoc(pos.z);

    g_xObjectsPenetrated   = info.GetObjectsPenetrated();
    g_xDamagedOtherPlayers = info.GetDamagedOtherPlayers();

    const auto bDamageValid  = !!nRetVal;
    const auto bKillerValid  = killer > 0 && killer <= playerhelpers->GetMaxClients();
    const auto bWeaponValid  = weapon > 0 && gamehelpers->ReferenceToEntity(weapon) != nullptr;
    const auto bSelfDamage   = killer == victim;
    const auto nVictimHealth = GetClientHealth(pVictim);
    const auto nVictimTeam   = GetEntityTeam(pVictim);
    const auto nKillerTeam   = bKillerValid && (pKiller = gamehelpers->ReferenceToEntity(killer)) != nullptr ? GetEntityTeam(pKiller) : 1;

    g_bInHook = true;

    for (const auto& hook : m_pHooks)
    {
        if (!hook->m_pFunction->IsRunnable())
            continue;

        /* filters */

        // filter if damage is not really done to player
        if (hook->m_nFilter[FilterDamageValid] && !bDamageValid)
            continue;

        // filter if killer is not a valid player
        if (hook->m_nFilter[FilterKillerValid] && !bKillerValid)
            continue;

        // filter if weapon is not a valid entity
        if (hook->m_nFilter[FilterWeaponValid] && !bWeaponValid)
            continue;

        // filter if damage type mismatch
        if (hook->m_nFilter[FilterDamageType] != -1 && (damagetype & hook->m_nFilter[FilterDamageType]) == 0)
            continue;

        // filter if killer is victim
        if (hook->m_nFilter[FilterSkipSelf] && bSelfDamage)
            continue;

        // filter if victim is dead in this damage
        if (hook->m_nFilter[FilterSkipDead] && nVictimHealth <= 0)
            continue;

        // filter if victim is not in team TE
        if (hook->m_nFilter[FilterVictimTeam] && nVictimTeam != hook->m_nFilter[FilterVictimTeam])
            continue;

        // filter if victim is not in team CT
        if (hook->m_nFilter[FilterKillerTeam] && nKillerTeam != hook->m_nFilter[FilterKillerTeam])
            continue;

        hook->m_pFunction->PushCell(victim);
        hook->m_pFunction->PushCell(killer);
        hook->m_pFunction->PushCell(inflictor);
        hook->m_pFunction->PushCell(weapon);
        hook->m_pFunction->PushFloat(damage);
        hook->m_pFunction->PushCell(damagetype);
        hook->m_pFunction->Execute(nullptr);
    }

    g_bInHook = false;

    RETURN_META_VALUE(MRES_IGNORED, nRetVal);
}

bool DamageManager::SDK_OnLoad(char* error, size_t maxlength, bool late)
{
    memset(g_iHookId, -1, sizeof(g_iHookId));

    sharesys->AddDependency(myself, "sdkhooks.ext", true, true);
    sharesys->AddDependency(myself, "sdktools.ext", true, true);
    sharesys->AddDependency(myself, "bintools.ext", true, true);
    SM_GET_IFACE(SDKHOOKS, g_pSDKHooks);
    SM_GET_IFACE(SDKTOOLS, g_pSDKTools);
    SM_GET_IFACE(BINTOOLS, g_pBinTools);

    IGameConfig* sdkhooks = nullptr;
    if (!gameconfs->LoadGameConfigFile("sdkhooks.games", &sdkhooks, error, maxlength))
    {
        smutils->Format(error, maxlength, "Failed to load SDKHooks gamedata: %s.", error);
        return false;
    }

    auto offset = -1;

    if (!sdkhooks->GetOffset("TraceAttack", &offset))
    {
        smutils->Format(error, maxlength, "Failed to load 'TraceAttack' offset.");
        return false;
    }
    SH_MANUALHOOK_RECONFIGURE(TraceAttack, offset, 0, 0);

    // OnTakeDamage
    if (!sdkhooks->GetOffset("OnTakeDamage_Alive", &offset))
    {
        smutils->Format(error, maxlength, "Failed to load 'OnTakeDamage_Alive' offset.");
        return false;
    }
    SH_MANUALHOOK_RECONFIGURE(OnTakeDamage, offset, 0, 0);

    gameconfs->CloseGameConfigFile(sdkhooks);

    if (!gameconfs->LoadGameConfigFile("damage.games", &g_pGameConf, error, maxlength))
    {
        smutils->Format(error, maxlength, "Failed to load DamageManager gamedata: %s.", error);
        return false;
    }

    playerhelpers->AddClientListener(this);
    plsys->AddPluginsListener(this);
    // g_pSDKHooks->AddEntityListener(this);

    g_pShareSys->AddNatives(myself, g_Natives);
    g_pShareSys->RegisterLibrary(myself, "DamageManager");

    m_pLastTrace = new CTrace;

    return true;
}

bool DamageManager::SDK_OnMetamodLoad(ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
    g_pGlobals = ismm->GetCGlobals();

    GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer, INTERFACEVERSION_VENGINESERVER);

    return true;
}

void DamageManager::NotifyInterfaceDrop(SMInterface* pInterface)
{
    if (strcmp(pInterface->GetInterfaceName(), SMINTERFACE_SDKHOOKS_NAME) == 0)
    {
        g_pSDKHooks = nullptr;
    }
    if (strcmp(pInterface->GetInterfaceName(), SMINTERFACE_SDKTOOLS_NAME) == 0)
    {
        g_pSDKTools = nullptr;
    }
    if (strcmp(pInterface->GetInterfaceName(), SMINTERFACE_BINTOOLS_NAME) == 0)
    {
        g_pBinTools = nullptr;
    }
}

bool DamageManager::QueryRunning(char* error, size_t maxlength)
{
    SM_CHECK_IFACE(SDKHOOKS, g_pSDKHooks);
    SM_CHECK_IFACE(SDKTOOLS, g_pSDKTools);
    SM_CHECK_IFACE(BINTOOLS, g_pBinTools);
    return true;
}

void DamageManager::SDK_OnUnload()
{
    delete m_pLastTrace;

    playerhelpers->RemoveClientListener(this);

    for (auto i = 0; i <= SM_MAXPLAYERS; i++)
    {
        for (auto n = 0; n < HOOK_MAX_COUNT; n++)
        {
            if (g_iHookId[i][n] != -1)
            {
                SH_REMOVE_HOOK_ID(g_iHookId[i][n]);
                g_iHookId[i][n] = -1;
            }
        }
    }

    for (size_t n = 0; n < m_pHooks.size(); ++n)
    {
        delete m_pHooks[n];
        m_pHooks.erase(m_pHooks.begin() + n);
        n--;
    }
}

void DamageManager::OnClientPutInServer(int client)
{
    const auto pPlayer = playerhelpers->GetGamePlayer(client);
    if (!pPlayer || pPlayer->IsSourceTV() || pPlayer->IsReplay())
    {
        return;
    }

    const auto pEntity = gamehelpers->ReferenceToEntity(client);

    g_iHookId[client][HookTraceAttack] = SH_ADD_MANUALHOOK(TraceAttack,
                                                           pEntity,
                                                           SH_MEMBER(&g_Damage, &DamageManager::Hook_TraceAttack),
                                                           true);

    g_iHookId[client][HookOnTakeDamage] = SH_ADD_MANUALHOOK(OnTakeDamage,
                                                            pEntity,
                                                            SH_MEMBER(&g_Damage, &DamageManager::
                                                                                     Hook_OnTakeDamage),
                                                            true);
}

void DamageManager::OnClientDisconnecting(int client)
{
    for (auto n = 0; n < HOOK_MAX_COUNT; n++)
    {
        if (g_iHookId[client][n] != -1)
        {
            SH_REMOVE_HOOK_ID(g_iHookId[client][n]);
            g_iHookId[client][n] = -1;
        }
    }
}

bool DamageManager::InstallHook(IPlugin* pPlugin, IPluginFunction* pFunction, const cell_t* filters)
{
    for (const auto& hook : m_pHooks)
    {
        if (hook->m_pPlugin == pPlugin && hook->m_pFunction == pFunction)
        {
            return false;
        }
    }

    const auto hook = new CPluginHook(pPlugin, pFunction, filters);
    m_pHooks.push_back(hook);

    return true;
}

bool DamageManager::RemoveHook(IPlugin* pPlugin, IPluginFunction* pFunction)
{
    for (size_t n = 0; n < m_pHooks.size(); ++n)
    {
        if (m_pHooks[n]->m_pPlugin == pPlugin && m_pHooks[n]->m_pFunction == pFunction)
        {
            delete m_pHooks[n];
            m_pHooks.erase(m_pHooks.begin() + n);
            n--;
            return true;
        }
    }
    return false;
}

void DamageManager::OnPluginUnloaded(IPlugin* plugin)
{
    for (size_t n = 0; n < m_pHooks.size(); ++n)
    {
        if (m_pHooks[n]->m_pPlugin == plugin)
        {
            delete m_pHooks[n];
            m_pHooks.erase(m_pHooks.begin() + n);
            n--;
        }
    }
}

static cell_t Native_InstallHook(IPluginContext* pContext, const cell_t* params)
{
    const auto pFunction = pContext->GetFunctionById(static_cast<funcid_t>(params[1]));
    if (pFunction == nullptr)
        return pContext->ThrowNativeError("Invalid hook callback!");

    const auto pPlugin = plsys->FindPluginByContext(pContext->GetContext());
    if (pPlugin == nullptr)
        return pContext->ThrowNativeError("Invalid plugin context!");

    if (params[0] < 3 || params[3] != FILTER_MAX_COUNT)
        return pContext->ThrowNativeError("Invalid filter params!");

    cell_t* pFilters;
    pContext->LocalToPhysAddr(params[2], &pFilters);

    return g_Damage.InstallHook(pPlugin, pFunction, pFilters);
}

static cell_t Native_RemoveHook(IPluginContext* pContext, const cell_t* params)
{
    const auto pFunction = pContext->GetFunctionById(static_cast<funcid_t>(params[1]));
    if (pFunction == nullptr)
        return pContext->ThrowNativeError("Invalid hook callback!");

    const auto pPlugin = plsys->FindPluginByContext(pContext->GetContext());
    if (pPlugin == nullptr)
        return pContext->ThrowNativeError("Invalid plugin context!");

    return g_Damage.RemoveHook(pPlugin, pFunction);
}

#define CreateDamageCallParams                                                                                        \
    CBaseEntity* pVictim = gamehelpers->ReferenceToEntity(params[1]);                                                 \
    if (!pVictim)                                                                                                     \
        return pContext->ThrowNativeError("Invalid entity index %d for victim", params[1]);                           \
                                                                                                                      \
    CBaseEntity* pInflictor = gamehelpers->ReferenceToEntity(params[2]);                                              \
    if (!pInflictor)                                                                                                  \
        return pContext->ThrowNativeError("Invalid entity index %d for inflictor", params[2]);                        \
                                                                                                                      \
    CBaseEntity* pKiller = nullptr;                                                                                   \
    if (params[3] != -1)                                                                                              \
    {                                                                                                                 \
        pKiller = gamehelpers->ReferenceToEntity(params[3]);                                                          \
        if (!pKiller)                                                                                                 \
        {                                                                                                             \
            return pContext->ThrowNativeError("Invalid entity index %d for attacker", params[3]);                     \
        }                                                                                                             \
    }                                                                                                                 \
                                                                                                                      \
    CBaseEntity* pWeapon = nullptr;                                                                                   \
    if (params[6] != -1)                                                                                              \
    {                                                                                                                 \
        pWeapon = gamehelpers->ReferenceToEntity(params[6]);                                                          \
        if (!pWeapon)                                                                                                 \
        {                                                                                                             \
            return pContext->ThrowNativeError("Invalid entity index %d for weapon", params[6]);                       \
        }                                                                                                             \
    }                                                                                                                 \
                                                                                                                      \
    cell_t* addr;                                                                                                     \
                                                                                                                      \
    if (pContext->LocalToPhysAddr(params[7], &addr) != SP_ERROR_NONE)                                                 \
    {                                                                                                                 \
        return pContext->ThrowNativeError("Could not read damageForce vector");                                       \
    }                                                                                                                 \
    Vector vecDamageForce;                                                                                            \
    if (addr != pContext->GetNullRef(SP_NULL_VECTOR))                                                                 \
    {                                                                                                                 \
        vecDamageForce.Init(sp_ctof(addr[0]), sp_ctof(addr[1]), sp_ctof(addr[2]));                                    \
    }                                                                                                                 \
    else                                                                                                              \
    {                                                                                                                 \
        vecDamageForce.Init();                                                                                        \
    }                                                                                                                 \
                                                                                                                      \
    if (pContext->LocalToPhysAddr(params[8], &addr) != SP_ERROR_NONE)                                                 \
    {                                                                                                                 \
        return pContext->ThrowNativeError("Could not read damagePosition vector");                                    \
    }                                                                                                                 \
    Vector vecDamagePosition;                                                                                         \
    if (addr != pContext->GetNullRef(SP_NULL_VECTOR))                                                                 \
    {                                                                                                                 \
        vecDamagePosition.Init(sp_ctof(addr[0]), sp_ctof(addr[1]), sp_ctof(addr[2]));                                 \
    }                                                                                                                 \
    else                                                                                                              \
    {                                                                                                                 \
        vecDamagePosition = vec3_origin;                                                                              \
    }                                                                                                                 \
                                                                                                                      \
    const auto          flDamage    = sp_ctof(params[4]);                                                             \
    const auto          iDamageType = params[5];                                                                      \
    CTakeDamageInfoHack info(pInflictor, pKiller, flDamage, iDamageType, pWeapon, vecDamageForce, vecDamagePosition); \
                                                                                                                      \
    unsigned char  vstk[sizeof(CBaseEntity*) + sizeof(CTakeDamageInfoHack&)];                                         \
    unsigned char* vptr = vstk;                                                                                       \
                                                                                                                      \
    *reinterpret_cast<CBaseEntity**>(vptr) = pVictim;                                                                 \
    vptr += sizeof(CBaseEntity*);                                                                                     \
    *reinterpret_cast<CTakeDamageInfoHack*&>(vptr) = info

static cell_t Native_TakeDamage(IPluginContext* pContext, const cell_t* params)
{
    static ICallWrapper* pCall = nullptr;
    if (pCall == nullptr)
    {
        void* pAddress;
        if (!g_pGameConf->GetMemSig("CBaseEntity::TakeDamage", &pAddress))
        {
            return pContext->ThrowNativeError("Could not find OnTakeDamage offset");
        }
        PassInfo pass[1];
        pass[0].type  = PassType_Object;
        pass[0].size  = sizeof(CTakeDamageInfoHack&);
        pass[0].flags = PASSFLAG_BYREF | PASSFLAG_OCTOR;

        pCall = g_pBinTools->CreateCall(pAddress, CallConv_ThisCall, nullptr, pass, 1);
    }

    CreateDamageCallParams;

    pCall->Execute(vstk, nullptr);
    return 0;
}

static cell_t Native_PassesDamageFilter(IPluginContext* pContext, const cell_t* params)
{
    static ICallWrapper* pCall = nullptr;
    if (pCall == nullptr)
    {
        cell_t offset;
        if (!g_pGameConf->GetOffset("CBaseEntity::PassesDamageFilter", &offset))
        {
            return pContext->ThrowNativeError("Could not find OnTakeDamage offset");
        }
        PassInfo pass[2];
        pass[0].type  = PassType_Object;
        pass[0].size  = sizeof(CTakeDamageInfoHack&);
        pass[0].flags = PASSFLAG_BYREF | PASSFLAG_OCTOR;
        pass[1].type  = PassType_Basic;
        pass[1].size  = sizeof(bool);
        pass[1].flags = PASSFLAG_BYVAL;

        pCall = g_pBinTools->CreateVCall(offset, 0, 0, &pass[1], &pass[0], 1);
    }

    CreateDamageCallParams;

    bool ret;
    pCall->Execute(vstk, &ret);
    return ret;
}

#define CHECKNATIVECALL \
    if (!g_bInHook)     \
    return pContext->ThrowNativeError("The native can only be called from a hook callback!")

#define CHECKTRACEVALID                      \
    if (!g_Damage.GetLastTrace()->IsValid()) \
    return pContext->ThrowNativeError("The last trace is invalid!")

static cell_t Native_GetDamageForce(IPluginContext* pContext, const cell_t* params)
{
    CHECKNATIVECALL;

    cell_t* pVec;
    pContext->LocalToPhysAddr(params[1], &pVec);
    pVec[0] = g_xDamageForce[0];
    pVec[1] = g_xDamageForce[1];
    pVec[2] = g_xDamageForce[2];
    return 1;
}

static cell_t Native_GetDamagePosition(IPluginContext* pContext, const cell_t* params)
{
    CHECKNATIVECALL;

    cell_t* pVec;
    pContext->LocalToPhysAddr(params[1], &pVec);
    pVec[0] = g_xDamagePosition[0];
    pVec[1] = g_xDamagePosition[1];
    pVec[2] = g_xDamagePosition[2];
    return 1;
}

static cell_t Native_GetDamagedOtherPlayers(IPluginContext* pContext, const cell_t* params)
{
    CHECKNATIVECALL;

    return g_xDamagedOtherPlayers;
}

static cell_t Native_GetObjectsPenetrated(IPluginContext* pContext, const cell_t* params)
{
    CHECKNATIVECALL;

    return g_xObjectsPenetrated;
}

static cell_t Native_GetTraceHitBox(IPluginContext* pContext, const cell_t* params)
{
    CHECKNATIVECALL;
    CHECKTRACEVALID;

    return g_Damage.GetLastTrace()->GetGameTrace()->hitbox;
}

static cell_t Native_GetTraceHitGroup(IPluginContext* pContext, const cell_t* params)
{
    CHECKNATIVECALL;
    CHECKTRACEVALID;

    return g_Damage.GetLastTrace()->GetGameTrace()->hitgroup;
}

static cell_t Native_GetLastHitGroup(IPluginContext* pContext, const cell_t* params)
{
    CHECKNATIVECALL;

    const auto pEntity = gamehelpers->ReferenceToEntity(params[1]);
    if (!pEntity)
        return pContext->ThrowNativeError("Invalid entity given!");

    static int offset         = GetSendPropOffset("CBaseCombatCharacter", "m_LastHitGroup");
    const auto m_LastHitGroup = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(pEntity) + offset);
    return m_LastHitGroup;
}

static cell_t Native_GetLastDamageHealth(IPluginContext* pContext, const cell_t* params)
{
    CHECKNATIVECALL;

    static cell_t offset = GetGameDataOffset("CCSPlayer::m_lastDamageHealth");
    if (offset == -1)
        return pContext->ThrowNativeError("Invalid offset, please check your GameData");

    const auto pEntity = gamehelpers->ReferenceToEntity(params[1]);
    if (!pEntity)
        return pContext->ThrowNativeError("Invalid entity given!");

    const auto m_lastDamageHealth = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(pEntity) + offset);

    return m_lastDamageHealth;
}

static cell_t Native_GetLastDamageArmor(IPluginContext* pContext, const cell_t* params)
{
    CHECKNATIVECALL;

    static cell_t offset = GetGameDataOffset("CCSPlayer::m_lastDamageArmor");
    if (offset == -1)
        return pContext->ThrowNativeError("Invalid offset, please check your GameData");

    const auto pEntity = gamehelpers->ReferenceToEntity(params[1]);
    if (!pEntity)
        return pContext->ThrowNativeError("Invalid entity given!");

    const auto m_lastDamageArmor = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(pEntity) + offset);

    return m_lastDamageArmor;
}

sp_nativeinfo_t g_Natives[] = {
    {"DamageManager_InstallHook", Native_InstallHook},
    {"DamageManager_RemoveHook", Native_RemoveHook},
    {"DamageManager_TakeDamage", Native_TakeDamage},
    {"DamageManager_PassesDamageFilter", Native_PassesDamageFilter},

    {"DamageManager_GetDamageForce", Native_GetDamageForce},
    {"DamageManager_GetDamagePosition", Native_GetDamagePosition},
    {"DamageManager_GetDamagedOtherPlayers", Native_GetDamagedOtherPlayers},
    {"DamageManager_GetObjectsPenetrated", Native_GetObjectsPenetrated},

    {"DamageManager_GetCurrentTraceHitBox", Native_GetTraceHitBox},
    {"DamageManager_GetCurrentTraceHitGroup", Native_GetTraceHitGroup},
    {"DamageManager_GetLastHitGroup", Native_GetLastHitGroup},
    {"DamageManager_GetLastDamageHealth", Native_GetLastDamageHealth},
    {"DamageManager_GetLastDamageArmor", Native_GetLastDamageArmor},
    {nullptr, nullptr},
};
