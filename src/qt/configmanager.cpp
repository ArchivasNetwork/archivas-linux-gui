#include "configmanager.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QDebug>

ConfigManager* ConfigManager::instance()
{
    static ConfigManager* inst = nullptr;
    if (!inst) {
        inst = new ConfigManager();
    }
    return inst;
}

ConfigManager::ConfigManager()
{
    // Determine config path based on platform
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty()) {
        configDir = QDir::homePath() + "/.archivas-core";
    }
    m_configPath = configDir + "/config.json";
    loadDefaults();
}

ConfigManager::~ConfigManager()
{
}

void ConfigManager::ensureConfigDir()
{
    QDir dir = QFileInfo(m_configPath).dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

QString ConfigManager::getConfigPath() const
{
    return m_configPath;
}

void ConfigManager::loadDefaults()
{
    // Node defaults - embedded Go code (no executable path needed)
    m_nodeConfig.executablePath = "";
    m_nodeConfig.network = "archivas-devnet-v4";
    m_nodeConfig.rpcBind = "127.0.0.1:8080";
    // Use QStandardPaths for cross-platform data directory
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataDir.isEmpty()) {
        appDataDir = QDir::homePath() + "/.archivas";
    }
    m_nodeConfig.dataDir = appDataDir + "/data";
    m_nodeConfig.bootnodes = "seed.archivas.ai:9090";
    m_nodeConfig.autoStart = true; // Auto-start by default

    // Farmer defaults - embedded Go code (no executable path needed)
    m_farmerConfig.executablePath = "";
    m_farmerConfig.plotsPath = appDataDir + "/plots";
    m_farmerConfig.farmerPrivkeyPath = appDataDir + "/farmer.key";
    m_farmerConfig.nodeUrl = "http://127.0.0.1:8080";
    m_farmerConfig.autoStart = true; // Auto-start by default

    // RPC defaults
    m_rpcConfig.url = "http://127.0.0.1:8080";
    m_rpcConfig.fallbackUrl = "https://seed.archivas.ai";
    m_rpcConfig.pollIntervalMs = 5000;

    // UI defaults
    m_uiConfig.theme = "dark";
    m_uiConfig.language = "en";
    m_uiConfig.minimizeToTray = true;
}

bool ConfigManager::loadConfig()
{
    QFile file(m_configPath);
    bool isFirstRun = !file.exists();
    
    if (isFirstRun) {
        ensureConfigDir();
        // First run: create directories and set up defaults
        setupFirstRun();
        return saveConfig(); // Create default config
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        return false;
    }

    return jsonToConfig(doc.object());
}

bool ConfigManager::saveConfig()
{
    ensureConfigDir();

    QFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonObject json = configToJson();
    QJsonDocument doc(json);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    return true;
}

QJsonObject ConfigManager::configToJson() const
{
    QJsonObject json;

    // Node config
    QJsonObject node;
    node["executable_path"] = m_nodeConfig.executablePath;
    node["network"] = m_nodeConfig.network;
    node["rpc_bind"] = m_nodeConfig.rpcBind;
    node["data_dir"] = m_nodeConfig.dataDir;
    node["bootnodes"] = m_nodeConfig.bootnodes;
    node["auto_start"] = m_nodeConfig.autoStart;
    json["node"] = node;

    // Farmer config
    QJsonObject farmer;
    farmer["executable_path"] = m_farmerConfig.executablePath;
    farmer["plots_path"] = m_farmerConfig.plotsPath;
    farmer["farmer_privkey_path"] = m_farmerConfig.farmerPrivkeyPath;
    farmer["node_url"] = m_farmerConfig.nodeUrl;
    farmer["auto_start"] = m_farmerConfig.autoStart;
    json["farmer"] = farmer;

    // RPC config
    QJsonObject rpc;
    rpc["url"] = m_rpcConfig.url;
    rpc["fallback_url"] = m_rpcConfig.fallbackUrl;
    rpc["poll_interval_ms"] = m_rpcConfig.pollIntervalMs;
    json["rpc"] = rpc;

    // UI config
    QJsonObject ui;
    ui["theme"] = m_uiConfig.theme;
    ui["language"] = m_uiConfig.language;
    ui["minimize_to_tray"] = m_uiConfig.minimizeToTray;
    json["ui"] = ui;

    return json;
}

bool ConfigManager::jsonToConfig(const QJsonObject& json)
{
    // Node config
    if (json.contains("node") && json["node"].isObject()) {
        QJsonObject node = json["node"].toObject();
        if (node.contains("executable_path")) m_nodeConfig.executablePath = node["executable_path"].toString();
        if (node.contains("network")) m_nodeConfig.network = node["network"].toString();
        if (node.contains("rpc_bind")) m_nodeConfig.rpcBind = node["rpc_bind"].toString();
        if (node.contains("data_dir")) m_nodeConfig.dataDir = node["data_dir"].toString();
        if (node.contains("bootnodes")) m_nodeConfig.bootnodes = node["bootnodes"].toString();
        if (node.contains("auto_start")) m_nodeConfig.autoStart = node["auto_start"].toBool();
    }

    // Farmer config
    if (json.contains("farmer") && json["farmer"].isObject()) {
        QJsonObject farmer = json["farmer"].toObject();
        if (farmer.contains("executable_path")) m_farmerConfig.executablePath = farmer["executable_path"].toString();
        if (farmer.contains("plots_path")) m_farmerConfig.plotsPath = farmer["plots_path"].toString();
        if (farmer.contains("farmer_privkey_path")) m_farmerConfig.farmerPrivkeyPath = farmer["farmer_privkey_path"].toString();
        if (farmer.contains("node_url")) m_farmerConfig.nodeUrl = farmer["node_url"].toString();
        if (farmer.contains("auto_start")) m_farmerConfig.autoStart = farmer["auto_start"].toBool();
    }

    // RPC config
    if (json.contains("rpc") && json["rpc"].isObject()) {
        QJsonObject rpc = json["rpc"].toObject();
        if (rpc.contains("url")) m_rpcConfig.url = rpc["url"].toString();
        if (rpc.contains("fallback_url")) m_rpcConfig.fallbackUrl = rpc["fallback_url"].toString();
        if (rpc.contains("poll_interval_ms")) m_rpcConfig.pollIntervalMs = rpc["poll_interval_ms"].toInt();
    }

    // UI config
    if (json.contains("ui") && json["ui"].isObject()) {
        QJsonObject ui = json["ui"].toObject();
        if (ui.contains("theme")) m_uiConfig.theme = ui["theme"].toString();
        if (ui.contains("language")) m_uiConfig.language = ui["language"].toString();
        if (ui.contains("minimize_to_tray")) m_uiConfig.minimizeToTray = ui["minimize_to_tray"].toBool();
    }

    return true;
}

void ConfigManager::setNodeConfig(const NodeConfig& config)
{
    m_nodeConfig = config;
}

void ConfigManager::setFarmerConfig(const FarmerConfig& config)
{
    m_farmerConfig = config;
}

void ConfigManager::setRpcConfig(const RpcConfig& config)
{
    m_rpcConfig = config;
}

void ConfigManager::setUiConfig(const UiConfig& config)
{
    m_uiConfig = config;
}

void ConfigManager::setupFirstRun()
{
    // Create all necessary directories
    ensureDirectories();
    
    // Generate farmer key if it doesn't exist
    ensureFarmerKey();
}

bool ConfigManager::ensureDirectories()
{
    bool success = true;
    
    // Create data directory (full path)
    QDir dataDir;
    if (!dataDir.mkpath(m_nodeConfig.dataDir)) {
        qDebug() << "Failed to create data directory:" << m_nodeConfig.dataDir;
        success = false;
    } else {
        qDebug() << "Created data directory:" << m_nodeConfig.dataDir;
    }
    
    // Create plots directory (full path)
    QDir plotsDir;
    if (!plotsDir.mkpath(m_farmerConfig.plotsPath)) {
        qDebug() << "Failed to create plots directory:" << m_farmerConfig.plotsPath;
        success = false;
    } else {
        qDebug() << "Created plots directory:" << m_farmerConfig.plotsPath;
    }
    
    // Create farmer key directory (parent directory of key file)
    QFileInfo keyInfo(m_farmerConfig.farmerPrivkeyPath);
    QDir keyDir = keyInfo.absoluteDir();
    if (!keyDir.exists()) {
        if (!keyDir.mkpath(".")) {
            qDebug() << "Failed to create farmer key directory:" << keyDir.absolutePath();
            success = false;
        } else {
            qDebug() << "Created farmer key directory:" << keyDir.absolutePath();
        }
    }
    
    return success;
}

bool ConfigManager::ensureFarmerKey()
{
    QFile keyFile(m_farmerConfig.farmerPrivkeyPath);
    if (keyFile.exists()) {
        return true; // Key already exists
    }
    
    // Key doesn't exist - we'll generate it via the farmer when it starts
    // For now, just ensure the directory exists
    QDir keyDir = QFileInfo(m_farmerConfig.farmerPrivkeyPath).dir();
    if (!keyDir.exists()) {
        keyDir.mkpath(".");
    }
    
    // Note: The farmer will generate the key on first run if it doesn't exist
    // This is handled by the Go farmer code
    return true;
}

