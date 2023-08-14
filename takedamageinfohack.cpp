#include "takedamageinfohack.h"
#include "extension.h"
#include <iplayerinfo.h>

const Vector vec3_origin(0, 0, 0);

CTakeDamageInfo::CTakeDamageInfo()
{
}

CTakeDamageInfoHack::CTakeDamageInfoHack(CBaseEntity* pInflictor, CBaseEntity* pAttacker, float flDamage,
                                         int bitsDamageType, CBaseEntity* pWeapon, Vector vecDamageForce,
                                         Vector vecDamagePosition)
{
    m_hInflictor = pInflictor;
    if (pAttacker)
    {
        SetAttacker(pAttacker);
    }
    else
    {
        SetAttacker(pInflictor);
    }

#if SOURCE_ENGINE >= SE_ORANGEBOX && SOURCE_ENGINE != SE_LEFT4DEAD
    m_hWeapon = pWeapon;
#endif

    m_flDamage = flDamage;

    m_flBaseDamage = BASEDAMAGE_NOT_SPECIFIED;

    m_bitsDamageType = bitsDamageType;

    m_flMaxDamage         = flDamage;
    m_vecDamageForce      = vecDamageForce;
    m_vecDamagePosition   = vecDamagePosition;
    m_vecReportedPosition = vec3_origin;
    m_iAmmoType           = -1;

#if SOURCE_ENGINE < SE_ORANGEBOX
    m_iCustomKillType = 0;
#else
    m_iDamageCustom = 0;
#endif

#if SOURCE_ENGINE == SE_CSS || SOURCE_ENGINE == SE_HL2DM || SOURCE_ENGINE == SE_DODS || SOURCE_ENGINE == SE_SDK2013 \
    || SOURCE_ENGINE == SE_BMS || SOURCE_ENGINE == SE_TF2 || SOURCE_ENGINE == SE_PVKII
    m_iDamagedOtherPlayers    = 0;
    m_iPlayerPenetrationCount = 0;
    m_flDamageBonus           = 0.0f;
    m_bForceFriendlyFire      = false;
#endif

#if SOURCE_ENGINE == SE_CSS || SOURCE_ENGINE == SE_HL2DM || SOURCE_ENGINE == SE_DODS || SOURCE_ENGINE == SE_TF2
    m_flDamageForForce = 0.f;
#endif

#if SOURCE_ENGINE == SE_TF2
    m_eCritType = kCritType_None;
#endif

#if SOURCE_ENGINE >= SE_ALIENSWARM
    m_flRadius = 0.0f;
#endif

#if SOURCE_ENGINE == SE_INSURGENCY || SOURCE_ENGINE == SE_DOI || SOURCE_ENGINE == SE_CSGO || SOURCE_ENGINE == SE_BLADE || SOURCE_ENGINE == SE_MCV
    m_iDamagedOtherPlayers = 0;
    m_iObjectsPenetrated   = 0;
    m_uiBulletID           = 0;
    m_uiRecoilIndex        = 0;
#endif
}

#if SOURCE_ENGINE == SE_CSGO
int CTakeDamageInfoHack::GetAttacker() const
{
    return m_CSGOAttacker.m_hHndl.IsValid() ? m_CSGOAttacker.m_hHndl.GetEntryIndex() : -1;
}

void CTakeDamageInfoHack::SetAttacker(CBaseEntity* pAttacker)
{
    m_CSGOAttacker.m_bNeedInit = false;
    m_CSGOAttacker.m_hHndl     = pAttacker;
    m_CSGOAttacker.m_bIsWorld  = true;

    auto entity = gamehelpers->EntityToBCompatRef(pAttacker);
    auto player = playerhelpers->GetGamePlayer(entity);
    if (player)
    {
        m_CSGOAttacker.m_bIsWorld     = false;
        m_CSGOAttacker.m_bIsPlayer    = true;
        m_CSGOAttacker.m_iClientIndex = player->GetIndex();
        m_CSGOAttacker.m_iUserId      = player->GetUserId();

        auto playerinfo = player->GetPlayerInfo();
        if (!playerinfo)
        {
            return;
        }
        m_CSGOAttacker.m_iTeamChecked = playerinfo->GetTeamIndex();
        m_CSGOAttacker.m_iTeamNum     = playerinfo->GetTeamIndex();
    }
}
#else
int CTakeDamageInfoHack::GetAttacker() const
{
    return m_hAttacker.IsValid() ? m_hAttacker.GetEntryIndex() : -1;
}

void CTakeDamageInfoHack::SetAttacker(CBaseEntity* pAttacker)
{
    CTakeDamageInfo::SetAttacker(pAttacker);
}
#endif
