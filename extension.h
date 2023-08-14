#ifndef _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_

#include "smsdk_ext.h"

#include <extensions/ISDKHooks.h>

class CTakeDamageInfoHack;
class CPluginHook;
class CTrace;

class DamageManager : public SDKExtension, public ISMEntityListener, public IClientListener, public IPluginsListener
{
public:
    bool SDK_OnMetamodLoad(ISmmAPI* ismm, char* error, size_t maxlen, bool late) override;
    bool SDK_OnLoad(char* error, size_t maxlength, bool late) override;
    void SDK_OnUnload() override;
    bool QueryRunning(char* error, size_t maxlength) override;
    void NotifyInterfaceDrop(SMInterface* pInterface) override;
    void OnClientPutInServer(int client) override;
    void OnClientDisconnecting(int client) override;
    void OnPluginUnloaded(IPlugin* plugin) override;

private:
    void Hook_TraceAttack(CTakeDamageInfoHack&, const Vector&, CGameTrace*) const;
    int  Hook_OnTakeDamage(CTakeDamageInfoHack&) const;

public:
    bool    InstallHook(IPlugin* pRuntime, IPluginFunction* pFunction, const cell_t* filters);
    bool    RemoveHook(IPlugin* pRuntime, IPluginFunction* pFunction);
    CTrace* GetLastTrace() const
    {
        return m_pLastTrace;
    }

private:
    std::vector<CPluginHook*> m_pHooks;
    CTrace*                   m_pLastTrace = nullptr;
};

#endif
