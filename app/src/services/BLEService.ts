import { BleManager, Device, Subscription } from 'react-native-ble-plx';
import { PermissionsAndroid, Platform } from 'react-native';
import {
	AudioConfig,
	DeviceInfo,
	ControlCommand,
	BLE_UUIDS,
	BLE_MTU_SIZE,
} from '../types/ble.types';

/**
 * BLE Service for connecting to ESP32 baby monitor device
 * Implements singleton pattern for global access
 */
class BLEService {
	private manager: BleManager | null = null;
	private connectedDevice: Device | null = null;
	private audioSubscription: Subscription | null = null;
	private disconnectSubscription: Subscription | null = null;
	private scanTimeout: ReturnType<typeof setTimeout> | null = null;
	private onDisconnectCallback: (() => void) | null = null;

	/**
	 * Initialize the BLE manager
	 * Must be called before using any other methods
	 */
	init(): void {
		if (this.manager) {
			console.log('[BLEService] Manager already initialized');
			return;
		}

		console.log('[BLEService] Initializing BLE Manager');
		this.manager = new BleManager();
	}

	/**
	 * Request Bluetooth permissions (Android only)
	 * iOS permissions are handled via Info.plist
	 */
	async requestPermissions(): Promise<boolean> {
		if (Platform.OS !== 'android') {
			console.log('[BLEService] iOS - permissions handled via Info.plist');
			return true;
		}

		try {
			console.log('[BLEService] Requesting Android Bluetooth permissions');

			if (Platform.Version >= 31) {
				// Android 12+ requires BLUETOOTH_SCAN and BLUETOOTH_CONNECT
				const granted = await PermissionsAndroid.requestMultiple([
					PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
					PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
					PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
				]);

				const allGranted =
					granted['android.permission.BLUETOOTH_SCAN'] === PermissionsAndroid.RESULTS.GRANTED &&
					granted['android.permission.BLUETOOTH_CONNECT'] === PermissionsAndroid.RESULTS.GRANTED &&
					granted['android.permission.ACCESS_FINE_LOCATION'] === PermissionsAndroid.RESULTS.GRANTED;

				console.log('[BLEService] Android 12+ permissions granted:', allGranted);
				return allGranted;
			} else {
				// Android 11 and below
				const granted = await PermissionsAndroid.request(
					PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION
				);

				const isGranted = granted === PermissionsAndroid.RESULTS.GRANTED;
				console.log('[BLEService] Android <12 location permission granted:', isGranted);
				return isGranted;
			}
		} catch (error) {
			console.error('[BLEService] Permission request error:', error);
			throw new Error(`Failed to request permissions: ${error}`);
		}
	}

	/**
	 * Scan for devices with service UUID 0x1900
	 * @param onDeviceFound - Callback invoked when a device is found
	 * @param timeoutMs - Scan timeout in milliseconds (default: 10000)
	 */
	async scanForDevices(
		onDeviceFound: (device: Device) => void,
		timeoutMs: number = 10000
	): Promise<void> {
		if (!this.manager) {
			throw new Error('BLE Manager not initialized. Call init() first.');
		}

		console.log(`[BLEService] Starting scan for ${timeoutMs}ms`);

		try {
			// Check if Bluetooth is powered on
			const state = await this.manager.state();
			console.log('[BLEService] Bluetooth state:', state);

			if (state !== 'PoweredOn') {
				throw new Error(`Bluetooth is not powered on. Current state: ${state}`);
			}

			// Start scanning for devices with the baby monitor service UUID
			this.manager.startDeviceScan(
				[BLE_UUIDS.SERVICE],
				{ allowDuplicates: false },
				(error, device) => {
					if (error) {
						console.error('[BLEService] Scan error:', error);
						this.stopScan();
						throw error;
					}

					if (device) {
						console.log('[BLEService] Device found:', device.id, device.name);
						onDeviceFound(device);
					}
				}
			);

			// Set timeout to stop scanning
			this.scanTimeout = setTimeout(() => {
				console.log('[BLEService] Scan timeout reached');
				this.stopScan();
			}, timeoutMs);
		} catch (error) {
			console.error('[BLEService] Failed to start scan:', error);
			throw new Error(`Failed to start scan: ${error}`);
		}
	}

	/**
	 * Stop scanning for devices
	 */
	stopScan(): void {
		if (!this.manager) {
			return;
		}

		console.log('[BLEService] Stopping scan');
		this.manager.stopDeviceScan();

		if (this.scanTimeout) {
			clearTimeout(this.scanTimeout);
			this.scanTimeout = null;
		}
	}

	/**
	 * Connect to a device and discover services
	 * @param device - The device to connect to
	 */
	async connect(device: Device): Promise<void> {
		if (!this.manager) {
			throw new Error('BLE Manager not initialized. Call init() first.');
		}

		try {
			console.log('[BLEService] Connecting to device:', device.id);

			// Connect to the device
			this.connectedDevice = await this.manager.connectToDevice(device.id);
			console.log('[BLEService] Connected successfully');

			// Setup disconnect monitoring
			this.setupDisconnectMonitoring();

			// Discover all services and characteristics
			console.log('[BLEService] Discovering services and characteristics');
			await this.connectedDevice.discoverAllServicesAndCharacteristics();
			console.log('[BLEService] Services discovered');

			// List ALL services and characteristics
			const services = await device.services();
			console.log('[BLEService] All services:');
			for (const service of services) {
				console.log(`  Service: ${service.uuid}`);
				const characteristics = await service.characteristics();
				for (const char of characteristics) {
					console.log(`    Char: ${char.uuid}, properties:`, char.isReadable, char.isWritableWithResponse, char.isNotifiable);
				}
			}

			// Request MTU negotiation
			console.log('[BLEService] Requesting MTU:', BLE_MTU_SIZE);
			try {
				const mtu = await this.connectedDevice.requestMTU(BLE_MTU_SIZE);
				console.log('[BLEService] MTU negotiated:', mtu);
			} catch (error) {
				console.warn('[BLEService] MTU negotiation failed (continuing anyway):', error);
			}

			console.log('[BLEService] Device ready');
		} catch (error) {
			console.error('[BLEService] Connection failed:', error);
			this.connectedDevice = null;
			throw new Error(`Failed to connect to device: ${error}`);
		}
	}

	/**
	 * Disconnect from the connected device
	 */
	async disconnect(): Promise<void> {
		if (!this.connectedDevice) {
			console.log('[BLEService] No device connected');
			return;
		}

		try {
			console.log('[BLEService] Disconnecting from device:', this.connectedDevice.id);

			// Remove disconnect monitoring
			if (this.disconnectSubscription) {
				this.disconnectSubscription.remove();
				this.disconnectSubscription = null;
			}

			// Cancel device connection
			await this.manager?.cancelDeviceConnection(this.connectedDevice.id);
			this.connectedDevice = null;

			console.log('[BLEService] Disconnected successfully');
		} catch (error) {
			console.error('[BLEService] Disconnect error:', error);
			throw new Error(`Failed to disconnect: ${error}`);
		}
	}


	/**
	 * Check if a device is currently connected
	 * @returns true if connected, false otherwise
	 */
	isConnected(): boolean {
		return this.connectedDevice !== null;
	}

	/**
	 * Get the currently connected device
	 * @returns The connected device or null
	 */
	getConnectedDevice(): Device | null {
		return this.connectedDevice;
	}

	/**
	 * Set callback for disconnect events
	 * @param callback - Function to call when device disconnects
	 */
	onDisconnect(callback: () => void): void {
		this.onDisconnectCallback = callback;
	}

	async readTemperature(): Promise<number> {
		if (!this.connectedDevice) {
			throw new Error('No device connected');
		}

		try {
			console.log('[BLEService] Reading temperature');

			const characteristic = await this.connectedDevice.readCharacteristicForService(
				BLE_UUIDS.SERVICE,
				BLE_UUIDS.TEMPERATURE
			);

			if (!characteristic.value) {
				throw new Error('No data received');
			}

			const data = this.base64ToUint8Array(characteristic.value);

			// ESP32 sends a float (4 bytes, little-endian)
			const buffer = new ArrayBuffer(4);
			const view = new DataView(buffer);
			for (let i = 0; i < 4; i++) {
				view.setUint8(i, data[i]);
			}
			const temperature = view.getFloat32(0, true); // little-endian

			console.log('[BLEService] Temperature:', temperature);
			return temperature;
		} catch (error) {
			console.error('[BLEService] Failed to read temperature:', error);
			throw new Error(`Failed to read temperature: ${error}`);
		}
	}

	async setLED(on: boolean): Promise<void> {
		if (!this.connectedDevice) {
			throw new Error('No device connected');
		}

		const data = new Uint8Array([on ? 1 : 0]);
		const base64Data = this.uint8ArrayToBase64(data);

		await this.connectedDevice.writeCharacteristicWithResponseForService(
			BLE_UUIDS.SERVICE,
			BLE_UUIDS.LED,
			base64Data
		);
	}

	/**
	 * Setup monitoring for device disconnection
	 * @private
	 */
	private setupDisconnectMonitoring(): void {
		if (!this.connectedDevice || !this.manager) {
			return;
		}

		this.disconnectSubscription = this.manager.onDeviceDisconnected(
			this.connectedDevice.id,
			(error, device) => {
				console.log('[BLEService] Device disconnected:', device?.id);

				if (error) {
					console.error('[BLEService] Disconnect error:', error);
				}

				// Clean up
				this.connectedDevice = null;

				// Notify callback
				if (this.onDisconnectCallback) {
					this.onDisconnectCallback();
				}
			}
		);
	}

	/**
	 * Convert base64 string to Uint8Array
	 * @private
	 */
	private base64ToUint8Array(base64: string): Uint8Array {
		const binaryString = atob(base64);
		const bytes = new Uint8Array(binaryString.length);
		for (let i = 0; i < binaryString.length; i++) {
			bytes[i] = binaryString.charCodeAt(i);
		}
		return bytes;
	}

	/**
	 * Convert Uint8Array to base64 string
	 * @private
	 */
	private uint8ArrayToBase64(data: Uint8Array): string {
		let binaryString = '';
		for (let i = 0; i < data.length; i++) {
			binaryString += String.fromCharCode(data[i]);
		}
		return btoa(binaryString);
	}

	/**
	 * Destroy the BLE manager and clean up resources
	 */
	destroy(): void {
		console.log('[BLEService] Destroying BLE Manager');

		this.stopScan();

		if (this.disconnectSubscription) {
			this.disconnectSubscription.remove();
			this.disconnectSubscription = null;
		}

		if (this.manager) {
			this.manager.destroy();
			this.manager = null;
		}

		this.connectedDevice = null;
		this.onDisconnectCallback = null;
	}
}

// Export singleton instance
export default new BLEService();
