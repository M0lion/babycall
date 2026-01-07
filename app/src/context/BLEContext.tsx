import React, { createContext, useContext, useState, useEffect, useCallback } from 'react';
import { Device } from 'react-native-ble-plx';
import { Alert } from 'react-native';
import BLEService from '../services/BLEService';
import AudioService from '../services/AudioService';
import AudioProtocol, { AudioProtocolStats } from '../services/AudioProtocol';
import { DeviceInfo, AudioConfig, ControlCommand } from '../types/ble.types';

/**
 * BLE Connection State
 */
interface BLEContextState {
  // Connection state
  isConnected: boolean;
  connectedDevice: Device | null;
  isScanning: boolean;
  isStreaming: boolean;

  // Device information
  deviceInfo: DeviceInfo | null;
  audioConfig: AudioConfig | null;

  // Statistics
  audioStats: {
    framesReceived: number;
    packetLossRate: number;
    bufferHealth: number;
    batteryLevel: number;
    uptime: number;
  };

  // Methods
  requestPermissions: () => Promise<boolean>;
  startScan: (onDeviceFound: (device: Device) => void) => Promise<void>;
  stopScan: () => void;
  connectToDevice: (device: Device) => Promise<void>;
  disconnect: () => Promise<void>;
  startStreaming: () => Promise<void>;
  stopStreaming: () => Promise<void>;
  setVolume: (volume: number) => void;
  refreshDeviceInfo: () => Promise<void>;
}

const BLEContext = createContext<BLEContextState | undefined>(undefined);

/**
 * BLE Context Provider Component
 */
export const BLEProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [isConnected, setIsConnected] = useState(false);
  const [connectedDevice, setConnectedDevice] = useState<Device | null>(null);
  const [isScanning, setIsScanning] = useState(false);
  const [isStreaming, setIsStreaming] = useState(false);
  const [deviceInfo, setDeviceInfo] = useState<DeviceInfo | null>(null);
  const [audioConfig, setAudioConfig] = useState<AudioConfig | null>(null);
  const [audioStats, setAudioStats] = useState({
    framesReceived: 0,
    packetLossRate: 0,
    bufferHealth: 0,
    batteryLevel: 0,
    uptime: 0,
  });

  // Initialize BLE Service on mount
  useEffect(() => {
    console.log('[BLEContext] Initializing BLE Service');
    BLEService.init();

    // Setup disconnect callback
    BLEService.onDisconnect(() => {
      console.log('[BLEContext] Device disconnected');
      handleDisconnect();
      Alert.alert('Device Disconnected', 'The baby monitor has been disconnected.');
    });

    return () => {
      // Cleanup on unmount
      BLEService.destroy();
      AudioService.dispose();
    };
  }, []);

  // Update stats periodically when streaming
  useEffect(() => {
    if (!isStreaming) {
      return;
    }

    const interval = setInterval(() => {
      const protocolStats = AudioProtocol.getStats();
      const audioServiceStats = AudioService.getStats();

      setAudioStats((prev) => ({
        ...prev,
        framesReceived: protocolStats.totalFrames,
        packetLossRate: protocolStats.packetLossRate,
        bufferHealth: audioServiceStats.bufferHealth,
      }));
    }, 500); // Update every 500ms

    return () => clearInterval(interval);
  }, [isStreaming]);

  // Refresh device info periodically when connected
  useEffect(() => {
    if (!isConnected) {
      return;
    }

    const refreshInfo = async () => {
      try {
        const info = await BLEService.readDeviceInfo();
        setDeviceInfo(info);
        setAudioStats((prev) => ({
          ...prev,
          batteryLevel: info.batteryLevel,
          uptime: info.uptime,
        }));
      } catch (error) {
        console.error('[BLEContext] Failed to refresh device info:', error);
      }
    };

    // Initial refresh
    refreshInfo();

    // Refresh every 5 seconds
    const interval = setInterval(refreshInfo, 5000);

    return () => clearInterval(interval);
  }, [isConnected]);

  /**
   * Request Bluetooth permissions
   */
  const requestPermissions = useCallback(async (): Promise<boolean> => {
    try {
      const granted = await BLEService.requestPermissions();
      if (!granted) {
        Alert.alert(
          'Permissions Required',
          'Bluetooth permissions are required to scan for devices.'
        );
      }
      return granted;
    } catch (error) {
      Alert.alert('Permission Error', `Failed to request permissions: ${error}`);
      return false;
    }
  }, []);

  /**
   * Start scanning for devices
   */
  const startScan = useCallback(
    async (onDeviceFound: (device: Device) => void): Promise<void> => {
      try {
        setIsScanning(true);
        await BLEService.scanForDevices(onDeviceFound, 10000);
      } catch (error) {
        Alert.alert('Scan Error', `Failed to start scan: ${error}`);
        setIsScanning(false);
      }
    },
    []
  );

  /**
   * Stop scanning for devices
   */
  const stopScan = useCallback(() => {
    BLEService.stopScan();
    setIsScanning(false);
  }, []);

  /**
   * Connect to a device
   */
  const connectToDevice = useCallback(async (device: Device): Promise<void> => {
    try {
      // Stop scanning first
      stopScan();

      // Connect to device
      await BLEService.connect(device);
      setConnectedDevice(device);
      setIsConnected(true);

      // Read device info and audio config
      const [info, config] = await Promise.all([
        BLEService.readDeviceInfo(),
        BLEService.readAudioConfig(),
      ]);

      setDeviceInfo(info);
      setAudioConfig(config);
      setAudioStats((prev) => ({
        ...prev,
        batteryLevel: info.batteryLevel,
        uptime: info.uptime,
      }));

      // Initialize audio service with sample rate
      AudioService.init(config.sampleRate);

      Alert.alert('Connected', `Connected to ${device.name || device.id}`);
    } catch (error) {
      Alert.alert('Connection Error', `Failed to connect to device: ${error}`);
      handleDisconnect();
    }
  }, [stopScan]);

  /**
   * Disconnect from device
   */
  const disconnect = useCallback(async (): Promise<void> => {
    try {
      // Stop streaming first if active
      if (isStreaming) {
        await stopStreaming();
      }

      await BLEService.disconnect();
      handleDisconnect();
    } catch (error) {
      Alert.alert('Disconnect Error', `Failed to disconnect: ${error}`);
      handleDisconnect(); // Force disconnect anyway
    }
  }, [isStreaming]);

  /**
   * Handle disconnect cleanup
   */
  const handleDisconnect = useCallback(() => {
    setIsConnected(false);
    setConnectedDevice(null);
    setIsStreaming(false);
    setDeviceInfo(null);
    setAudioConfig(null);
    setAudioStats({
      framesReceived: 0,
      packetLossRate: 0,
      bufferHealth: 0,
      batteryLevel: 0,
      uptime: 0,
    });
    AudioService.stop();
    AudioProtocol.reset();
  }, []);

  /**
   * Start audio streaming
   */
  const startStreaming = useCallback(async (): Promise<void> => {
    if (!isConnected) {
      Alert.alert('Not Connected', 'Please connect to a device first.');
      return;
    }

    try {
      // Reset protocol and audio service
      AudioProtocol.reset();
      AudioService.stop();

      // Start monitoring audio data
      await BLEService.startAudioStream((data: Uint8Array) => {
        // Process incoming data through protocol
        const frame = AudioProtocol.processIncomingData(data);

        if (frame) {
          // Play the audio frame
          AudioService.playPCMFrame(frame);
        }
      });

      // Send start stream command to device
      await BLEService.sendControlCommand(ControlCommand.START_STREAM);

      setIsStreaming(true);
      console.log('[BLEContext] Audio streaming started');
    } catch (error) {
      Alert.alert('Streaming Error', `Failed to start streaming: ${error}`);
      setIsStreaming(false);
    }
  }, [isConnected]);

  /**
   * Stop audio streaming
   */
  const stopStreaming = useCallback(async (): Promise<void> => {
    try {
      // Send stop stream command to device
      await BLEService.sendControlCommand(ControlCommand.STOP_STREAM);

      // Stop monitoring audio data
      BLEService.stopAudioStream();

      // Stop audio playback
      AudioService.stop();

      setIsStreaming(false);
      console.log('[BLEContext] Audio streaming stopped');
    } catch (error) {
      Alert.alert('Streaming Error', `Failed to stop streaming: ${error}`);
      // Force stop anyway
      BLEService.stopAudioStream();
      AudioService.stop();
      setIsStreaming(false);
    }
  }, []);

  /**
   * Set playback volume
   */
  const setVolume = useCallback((volume: number) => {
    AudioService.setVolume(volume);
  }, []);

  /**
   * Manually refresh device info
   */
  const refreshDeviceInfo = useCallback(async (): Promise<void> => {
    if (!isConnected) {
      return;
    }

    try {
      const info = await BLEService.readDeviceInfo();
      setDeviceInfo(info);
      setAudioStats((prev) => ({
        ...prev,
        batteryLevel: info.batteryLevel,
        uptime: info.uptime,
      }));
    } catch (error) {
      Alert.alert('Error', `Failed to refresh device info: ${error}`);
    }
  }, [isConnected]);

  const value: BLEContextState = {
    isConnected,
    connectedDevice,
    isScanning,
    isStreaming,
    deviceInfo,
    audioConfig,
    audioStats,
    requestPermissions,
    startScan,
    stopScan,
    connectToDevice,
    disconnect,
    startStreaming,
    stopStreaming,
    setVolume,
    refreshDeviceInfo,
  };

  return <BLEContext.Provider value={value}>{children}</BLEContext.Provider>;
};

/**
 * Hook to use BLE Context
 */
export const useBLE = (): BLEContextState => {
  const context = useContext(BLEContext);
  if (context === undefined) {
    throw new Error('useBLE must be used within a BLEProvider');
  }
  return context;
};
