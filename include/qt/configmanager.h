#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QString>
#include <QStringList>
#include <QStandardPaths>
#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>

struct NodeConfig {
    QString executablePath;
    QString network;
    QString rpcBind;
    QString dataDir;
    QString bootnodes;
    bool autoStart;
};

struct FarmerConfig {
    QString executablePath;
    QString plotsPath;
    QString farmerPrivkeyPath;
    QString nodeUrl;
    bool autoStart;
};

struct RpcConfig {
    QString url;
    QString fallbackUrl;
    int pollIntervalMs;
};

struct UiConfig {
    QString theme;
    QString language;
    bool minimizeToTray;
};

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    bool loadConfig();
    bool saveConfig();
    QString getConfigPath() const;

    // Getters
    NodeConfig getNodeConfig() const { return m_nodeConfig; }
    FarmerConfig getFarmerConfig() const { return m_farmerConfig; }
    RpcConfig getRpcConfig() const { return m_rpcConfig; }
    UiConfig getUiConfig() const { return m_uiConfig; }

    // Setters
    void setNodeConfig(const NodeConfig& config);
    void setFarmerConfig(const FarmerConfig& config);
    void setRpcConfig(const RpcConfig& config);
    void setUiConfig(const UiConfig& config);

    // Default config
    static ConfigManager* instance();
    void loadDefaults();
    
    // First-run setup
    void setupFirstRun();
    bool ensureDirectories();
    bool ensureFarmerKey();

private:
    QString m_configPath;
    NodeConfig m_nodeConfig;
    FarmerConfig m_farmerConfig;
    RpcConfig m_rpcConfig;
    UiConfig m_uiConfig;

    QJsonObject configToJson() const;
    bool jsonToConfig(const QJsonObject& json);
    void ensureConfigDir();
};

#endif // CONFIGMANAGER_H

