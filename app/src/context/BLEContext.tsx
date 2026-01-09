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

	temperature: number | null;
	led: boolean;

	// Methods
	requestPermissions: () => Promise<boolean>;
	startScan: (onDeviceFound: (device: Device) => void) => Promise<void>;
	stopScan: () => void;
	connectToDevice: (device: Device) => Promise<void>;
	disconnect: () => Promise<void>;
	toggleLed: () => Promise<void>;
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
	const [temperature, setTemperature] = useState<number | null>(null);
	const [led, setLed] = useState(false);
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
				const temperature = await BLEService.readTemperature();
				setTemperature(temperature);
			} catch (error) {
				console.error('[BLEContext] Failed to get temperature:', error);
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
			const temperature = await BLEService.readTemperature();

			setTemperature(temperature);

			AudioService.init();

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
		setTemperature(null);
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
	const toggleLed = useCallback(async (): Promise<void> => {
		if (!isConnected) {
			Alert.alert('Not Connected', 'Please connect to a device first.');
			return;
		}

		try {
			setLed(prevLed => {
				const newLed = !prevLed;
				console.log(`Led is ${prevLed} setting to ${newLed}`);
				BLEService.setLED(newLed);  // Note: fire-and-forget or handle separately
				return newLed;
			});
			startStreaming();
		} catch (error) {
			Alert.alert('Streaming Error', `Failed to start streaming: ${error}`);
		}
	}, [isConnected]);

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

			setIsStreaming(true);
			console.log('[BLEContext] Audio streaming started');
		} catch (error) {
			Alert.alert('Streaming Error', `Failed to start streaming: ${error}`);
			setIsStreaming(false);
		}
	}, [isConnected]);


	const value: BLEContextState = {
		isConnected,
		connectedDevice,
		isScanning,
		led,
		temperature,
		requestPermissions,
		startScan,
		stopScan,
		connectToDevice,
		disconnect,
		toggleLed,
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
