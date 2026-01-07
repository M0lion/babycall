import React, { useState, useEffect } from 'react';
import {
  StyleSheet,
  View,
  ScrollView,
  TouchableOpacity,
  ActivityIndicator,
  RefreshControl,
} from 'react-native';
import { Device } from 'react-native-ble-plx';
import { ThemedText } from '@/components/themed-text';
import { ThemedView } from '@/components/themed-view';
import { Colors } from '@/constants/theme';
import { useColorScheme } from '@/hooks/use-color-scheme';
import { useBLE } from '@/src/context/BLEContext';
import { IconSymbol } from '@/components/ui/icon-symbol';

/**
 * Device Scan Screen
 * Allows users to scan for and connect to ESP32 baby monitor devices
 */
export default function DeviceScanScreen() {
  const colorScheme = useColorScheme();
  const colors = Colors[colorScheme ?? 'light'];
  const {
    isConnected,
    connectedDevice,
    isScanning,
    requestPermissions,
    startScan,
    stopScan,
    connectToDevice,
    disconnect,
  } = useBLE();

  const [discoveredDevices, setDiscoveredDevices] = useState<Device[]>([]);
  const [permissionsGranted, setPermissionsGranted] = useState(false);
  const [connectingDeviceId, setConnectingDeviceId] = useState<string | null>(null);

  // Request permissions on mount
  useEffect(() => {
    const checkPermissions = async () => {
      const granted = await requestPermissions();
      setPermissionsGranted(granted);
    };
    checkPermissions();
  }, [requestPermissions]);

  // Handle scan start
  const handleStartScan = async () => {
    if (!permissionsGranted) {
      const granted = await requestPermissions();
      if (!granted) {
        return;
      }
      setPermissionsGranted(granted);
    }

    setDiscoveredDevices([]);
    await startScan((device) => {
      setDiscoveredDevices((prev) => {
        // Avoid duplicates
        const exists = prev.find((d) => d.id === device.id);
        if (exists) {
          // Update existing device (RSSI may have changed)
          return prev.map((d) => (d.id === device.id ? device : d));
        }
        return [...prev, device];
      });
    });

    // Auto-stop scanning after timeout
    setTimeout(() => {
      stopScan();
    }, 10000);
  };

  // Handle scan stop
  const handleStopScan = () => {
    stopScan();
  };

  // Handle device connection
  const handleConnect = async (device: Device) => {
    setConnectingDeviceId(device.id);
    try {
      await connectToDevice(device);
    } finally {
      setConnectingDeviceId(null);
    }
  };

  // Handle device disconnection
  const handleDisconnect = async () => {
    await disconnect();
    setDiscoveredDevices([]);
  };

  // Get signal strength indicator
  const getSignalStrength = (rssi: number | null): string => {
    if (rssi === null) return 'Unknown';
    if (rssi > -60) return 'Excellent';
    if (rssi > -70) return 'Good';
    if (rssi > -80) return 'Fair';
    return 'Weak';
  };

  // Get signal icon
  const getSignalIcon = (rssi: number | null): any => {
    if (rssi === null) return 'antenna.radiowaves.left.and.right.slash';
    if (rssi > -60) return 'wifi';
    if (rssi > -70) return 'wifi';
    if (rssi > -80) return 'wifi';
    return 'wifi';
  };

  return (
    <ThemedView style={styles.container}>
      <ScrollView
        style={styles.scrollView}
        contentContainerStyle={styles.scrollContent}
        refreshControl={
          <RefreshControl
            refreshing={isScanning}
            onRefresh={handleStartScan}
            tintColor={colors.tint}
          />
        }
      >
        {/* Header */}
        <View style={styles.header}>
          <ThemedText type="title" style={styles.title}>
            Baby Monitor Devices
          </ThemedText>
          <ThemedText style={styles.subtitle}>
            {isConnected
              ? 'Connected to device'
              : 'Scan for nearby ESP32 baby monitors'}
          </ThemedText>
        </View>

        {/* Permissions Section */}
        {!permissionsGranted && (
          <View style={[styles.section, { backgroundColor: colors.background }]}>
            <View style={styles.iconContainer}>
              <IconSymbol
                size={48}
                name="lock.shield"
                color={colors.tint}
              />
            </View>
            <ThemedText type="subtitle" style={styles.sectionTitle}>
              Bluetooth Permissions Required
            </ThemedText>
            <ThemedText style={styles.sectionDescription}>
              This app needs Bluetooth permissions to scan for and connect to baby monitor devices.
            </ThemedText>
            <TouchableOpacity
              style={[styles.primaryButton, { backgroundColor: colors.tint }]}
              onPress={async () => {
                const granted = await requestPermissions();
                setPermissionsGranted(granted);
              }}
            >
              <ThemedText style={styles.primaryButtonText}>
                Grant Permissions
              </ThemedText>
            </TouchableOpacity>
          </View>
        )}

        {/* Connected Device Section */}
        {isConnected && connectedDevice && (
          <View style={[styles.section, styles.connectedSection]}>
            <View style={styles.deviceHeader}>
              <IconSymbol
                size={32}
                name="checkmark.circle.fill"
                color="#4CAF50"
              />
              <View style={styles.deviceInfo}>
                <ThemedText type="defaultSemiBold" style={styles.deviceName}>
                  {connectedDevice.name || 'Unknown Device'}
                </ThemedText>
                <ThemedText style={styles.deviceId}>
                  {connectedDevice.id}
                </ThemedText>
              </View>
            </View>
            <TouchableOpacity
              style={[styles.disconnectButton, { borderColor: colors.tint }]}
              onPress={handleDisconnect}
            >
              <ThemedText style={[styles.disconnectButtonText, { color: colors.tint }]}>
                Disconnect
              </ThemedText>
            </TouchableOpacity>
          </View>
        )}

        {/* Scan Controls */}
        {permissionsGranted && !isConnected && (
          <View style={styles.scanControls}>
            {!isScanning ? (
              <TouchableOpacity
                style={[styles.primaryButton, { backgroundColor: colors.tint }]}
                onPress={handleStartScan}
              >
                <IconSymbol
                  size={20}
                  name="magnifyingglass"
                  color="#fff"
                  style={styles.buttonIcon}
                />
                <ThemedText style={styles.primaryButtonText}>
                  Start Scanning
                </ThemedText>
              </TouchableOpacity>
            ) : (
              <TouchableOpacity
                style={[styles.secondaryButton, { borderColor: colors.tint }]}
                onPress={handleStopScan}
              >
                <ActivityIndicator size="small" color={colors.tint} />
                <ThemedText style={[styles.secondaryButtonText, { color: colors.tint }]}>
                  Stop Scanning
                </ThemedText>
              </TouchableOpacity>
            )}
          </View>
        )}

        {/* Discovered Devices List */}
        {permissionsGranted && !isConnected && discoveredDevices.length > 0 && (
          <View style={styles.devicesSection}>
            <ThemedText type="subtitle" style={styles.devicesHeader}>
              Discovered Devices ({discoveredDevices.length})
            </ThemedText>
            {discoveredDevices.map((device) => (
              <View
                key={device.id}
                style={[styles.deviceCard, { backgroundColor: colors.background }]}
              >
                <View style={styles.deviceCardContent}>
                  <View style={styles.deviceCardHeader}>
                    <IconSymbol
                      size={24}
                      name="antenna.radiowaves.left.and.right"
                      color={colors.icon}
                    />
                    <View style={styles.deviceCardInfo}>
                      <ThemedText type="defaultSemiBold" style={styles.deviceCardName}>
                        {device.name || 'Unknown Device'}
                      </ThemedText>
                      <ThemedText style={styles.deviceCardId}>
                        {device.id}
                      </ThemedText>
                    </View>
                  </View>
                  <View style={styles.deviceCardFooter}>
                    <View style={styles.signalInfo}>
                      <IconSymbol
                        size={16}
                        name={getSignalIcon(device.rssi)}
                        color={colors.icon}
                      />
                      <ThemedText style={styles.signalText}>
                        {getSignalStrength(device.rssi)} ({device.rssi || 'N/A'} dBm)
                      </ThemedText>
                    </View>
                    <TouchableOpacity
                      style={[
                        styles.connectButton,
                        { backgroundColor: colors.tint },
                        connectingDeviceId === device.id && styles.connectingButton,
                      ]}
                      onPress={() => handleConnect(device)}
                      disabled={connectingDeviceId === device.id}
                    >
                      {connectingDeviceId === device.id ? (
                        <ActivityIndicator size="small" color="#fff" />
                      ) : (
                        <ThemedText style={styles.connectButtonText}>
                          Connect
                        </ThemedText>
                      )}
                    </TouchableOpacity>
                  </View>
                </View>
              </View>
            ))}
          </View>
        )}

        {/* No Devices Found */}
        {permissionsGranted && !isConnected && !isScanning && discoveredDevices.length === 0 && (
          <View style={styles.emptyState}>
            <IconSymbol
              size={64}
              name="antenna.radiowaves.left.and.right.slash"
              color={colors.icon}
            />
            <ThemedText type="subtitle" style={styles.emptyStateTitle}>
              No Devices Found
            </ThemedText>
            <ThemedText style={styles.emptyStateDescription}>
              Make sure your baby monitor is powered on and in range.
            </ThemedText>
          </View>
        )}
      </ScrollView>
    </ThemedView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  scrollView: {
    flex: 1,
  },
  scrollContent: {
    padding: 20,
  },
  header: {
    marginBottom: 24,
  },
  title: {
    marginBottom: 8,
  },
  subtitle: {
    opacity: 0.7,
  },
  section: {
    padding: 20,
    borderRadius: 12,
    marginBottom: 20,
    alignItems: 'center',
  },
  iconContainer: {
    marginBottom: 16,
  },
  sectionTitle: {
    marginBottom: 8,
    textAlign: 'center',
  },
  sectionDescription: {
    textAlign: 'center',
    opacity: 0.7,
    marginBottom: 20,
  },
  connectedSection: {
    backgroundColor: '#E8F5E9',
    borderWidth: 2,
    borderColor: '#4CAF50',
  },
  deviceHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 16,
    width: '100%',
  },
  deviceInfo: {
    marginLeft: 12,
    flex: 1,
  },
  deviceName: {
    fontSize: 18,
  },
  deviceId: {
    fontSize: 12,
    opacity: 0.6,
    marginTop: 4,
  },
  disconnectButton: {
    paddingVertical: 12,
    paddingHorizontal: 24,
    borderRadius: 8,
    borderWidth: 2,
    width: '100%',
    alignItems: 'center',
  },
  disconnectButtonText: {
    fontWeight: '600',
    fontSize: 16,
  },
  scanControls: {
    marginBottom: 20,
  },
  primaryButton: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: 16,
    paddingHorizontal: 24,
    borderRadius: 12,
  },
  primaryButtonText: {
    color: '#fff',
    fontWeight: '600',
    fontSize: 16,
  },
  buttonIcon: {
    marginRight: 8,
  },
  secondaryButton: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: 16,
    paddingHorizontal: 24,
    borderRadius: 12,
    borderWidth: 2,
    gap: 12,
  },
  secondaryButtonText: {
    fontWeight: '600',
    fontSize: 16,
  },
  devicesSection: {
    marginBottom: 20,
  },
  devicesHeader: {
    marginBottom: 16,
  },
  deviceCard: {
    borderRadius: 12,
    marginBottom: 12,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  deviceCardContent: {
    padding: 16,
  },
  deviceCardHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 12,
  },
  deviceCardInfo: {
    marginLeft: 12,
    flex: 1,
  },
  deviceCardName: {
    fontSize: 16,
  },
  deviceCardId: {
    fontSize: 11,
    opacity: 0.6,
    marginTop: 2,
  },
  deviceCardFooter: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
  },
  signalInfo: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
  },
  signalText: {
    fontSize: 12,
    opacity: 0.7,
  },
  connectButton: {
    paddingVertical: 8,
    paddingHorizontal: 20,
    borderRadius: 8,
    minWidth: 100,
    alignItems: 'center',
  },
  connectingButton: {
    opacity: 0.7,
  },
  connectButtonText: {
    color: '#fff',
    fontWeight: '600',
    fontSize: 14,
  },
  emptyState: {
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: 60,
  },
  emptyStateTitle: {
    marginTop: 16,
    marginBottom: 8,
  },
  emptyStateDescription: {
    textAlign: 'center',
    opacity: 0.7,
  },
});
