#include "archivasapplication.h"
#include <QStyle>
#include <QStyleFactory>
#include <QPalette>
#include <QColor>
#include <QIcon>
#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QPixmap>
#include <QPainter>
#include <QFont>

ArchivasApplication::ArchivasApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
    setApplicationName("archivas-core");
    setApplicationDisplayName("Archivas Core");
    setApplicationVersion("1.0.0");
    setOrganizationName("Archivas Network");
    setOrganizationDomain("archivas.ai");
    setDesktopFileName("archivas-core.desktop");

    setupStyle();
    setupIcon();
}

ArchivasApplication::~ArchivasApplication()
{
}

void ArchivasApplication::setupStyle()
{
    // Use Fusion style for a modern look
    setStyle(QStyleFactory::create("Fusion"));

    // Set dark palette
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(35, 35, 35));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);

    setPalette(darkPalette);
}

void ArchivasApplication::setupIcon()
{
    // Try to load icon from multiple sources
    QIcon icon;
    QString iconName = "archivas-core";
    
    // Try system icon cache first (for taskbar) - this is the preferred method
    QIcon systemIcon = QIcon::fromTheme(iconName);
    if (!systemIcon.isNull()) {
        icon = systemIcon;
    } else {
        // Load from user's local icon directory
        QString userIconDir = QDir::homePath() + "/.local/share/icons/hicolor";
        QList<int> sizes = {16, 22, 24, 32, 48, 64, 96, 128, 256, 512};
        
        for (int size : sizes) {
            QString iconPath = QString("%1/%2x%2/apps/%3.png")
                              .arg(userIconDir).arg(size).arg(iconName);
            if (QFile::exists(iconPath)) {
                icon.addFile(iconPath, QSize(size, size));
            }
        }
        
        // If still no icon, try embedded resources
        if (icon.isNull()) {
            QStringList resourcePaths = {
                ":/icons/archivas-core-512x512.png",
                ":/icons/archivas-core-256x256.png",
                ":/icons/archivas-core-128x128.png",
                ":/icons/archivas-core-64x64.png",
                ":/icons/archivas-core-48x48.png",
                ":/icons/archivas-core-32x32.png",
                ":/icons/archivas-core.png",
                ":/icons/icon.png",
            };
            
            for (const QString& path : resourcePaths) {
                if (QFile::exists(path)) {
                    icon.addFile(path);
                }
            }
            
            // Also try loading all sizes from resources
            for (int size : sizes) {
                QString resourcePath = QString(":/icons/archivas-core-%1x%1.png").arg(size);
                if (QFile::exists(resourcePath)) {
                    icon.addFile(resourcePath, QSize(size, size));
                }
            }
        }
        
        // Fallback to file system in resources directory
        if (icon.isNull()) {
            QString resourcesDir = QDir::currentPath() + "/resources/icons";
            QStringList filePaths = {
                resourcesDir + "/archivas-core.png",
                resourcesDir + "/archivas-core-512x512.png",
                resourcesDir + "/icon.png",
            };
            
            for (const QString& path : filePaths) {
                if (QFile::exists(path)) {
                    icon.addFile(path);
                    // Try to load all sizes from same directory
                    for (int size : sizes) {
                        QString sizePath = resourcesDir + QString("/archivas-core-%1x%1.png").arg(size);
                        if (QFile::exists(sizePath)) {
                            icon.addFile(sizePath, QSize(size, size));
                        }
                    }
                    break;
                }
            }
        }
    }
    
    // If no icon found, create a default one programmatically
    if (icon.isNull()) {
        // Create multiple sizes for better scaling
        QList<int> sizes = {16, 32, 48, 64, 128, 256, 512};
        
        for (int size : sizes) {
            QPixmap pixmap(size, size);
            pixmap.fill(Qt::transparent);
            
            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            
            int radius = (size * 240) / 512; // Scale based on 512px original
            int borderWidth = (size * 8) / 512;
            int centerX = size / 2;
            int centerY = size / 2;
            
            // Draw dark gray border circle
            painter.setPen(QPen(QColor(74, 74, 74), borderWidth));
            painter.setBrush(QBrush(QColor(45, 27, 78))); // Dark purple
            painter.drawEllipse(centerX - radius - borderWidth/2, 
                              centerY - radius - borderWidth/2,
                              (radius + borderWidth/2) * 2, 
                              (radius + borderWidth/2) * 2);
            
            // Draw dark purple circle
            painter.setPen(Qt::NoPen);
            painter.setBrush(QBrush(QColor(45, 27, 78))); // Dark purple #2d1b4e
            painter.drawEllipse(centerX - radius, centerY - radius, 
                              radius * 2, radius * 2);
            
            // Draw neon green "R"
            painter.setPen(QPen(QColor(0, 255, 136), size / 64, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); // Neon green #00ff88
            painter.setBrush(QBrush(QColor(0, 255, 136)));
            
            // Use a bold font for the R
            QFont font;
            font.setBold(true);
            font.setPixelSize(size * 320 / 512);
            painter.setFont(font);
            
            QRect textRect(0, 0, size, size);
            painter.drawText(textRect, Qt::AlignCenter, "R");
            
            icon.addPixmap(pixmap);
        }
    }
    
    setWindowIcon(icon);
}

