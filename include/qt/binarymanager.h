#ifndef BINARYMANAGER_H
#define BINARYMANAGER_H

#include <QString>

class BinaryManager
{
public:
    static BinaryManager* instance();
    
    // Get paths to extracted binaries
    QString getNodeBinaryPath();
    QString getFarmerBinaryPath();
    QString getBinariesDirectory() const;
    
    // Check if binaries are available
    bool areBinariesAvailable();
    
private:
    BinaryManager();
    ~BinaryManager();
    
    bool extractBinary(const QString& resourcePath, const QString& targetPath);
    
    QString m_binariesDir;
};

#endif // BINARYMANAGER_H

