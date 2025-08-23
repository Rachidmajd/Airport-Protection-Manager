// frontend/js/coordinateEntry.js
import { showNotification } from './ui.js';
import mapManager from './map.js';

class CoordinateEntryManager {
    constructor() {
        this.modal = null;
        this.previewMap = null;
        this.previewLayer = null;
        this.isValid = false;
    }

    init() {
        this.modal = document.getElementById('coordinate-entry-modal');
        this.setupEventHandlers();
    }

    setupEventHandlers() {
        // Open modal button
        const manualBtn = document.getElementById('manual-entry-btn');
        if (manualBtn) {
            manualBtn.addEventListener('click', () => this.showModal());
        }

        // Close modal
        document.getElementById('close-coordinate-modal')?.addEventListener('click', () => this.hideModal());
        document.getElementById('cancel-coordinate-entry')?.addEventListener('click', () => this.hideModal());

        // Validate button
        document.getElementById('validate-coordinates')?.addEventListener('click', () => this.validateAndPreview());

        // Form submission
        document.getElementById('coordinate-entry-form')?.addEventListener('submit', (e) => {
            e.preventDefault();
            this.createZoneFromCoordinates();
        });

        // Format change handler
        document.getElementById('coordinate-format')?.addEventListener('change', (e) => {
            this.updatePlaceholder(e.target.value);
        });
    }

    showModal() {
        if (this.modal) {
            this.modal.classList.remove('hidden');
            this.initPreviewMap();
        }
    }

    hideModal() {
        if (this.modal) {
            this.modal.classList.add('hidden');
            this.clearPreview();
        }
    }

    initPreviewMap() {
        if (!this.previewMap) {
            this.previewMap = L.map('coordinate-preview-map').setView([40.639751, -73.778925], 10);
            L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png').addTo(this.previewMap);
        }
    }

    parseCoordinates(text, format, geometryType) {
        const lines = text.trim().split('\n').filter(line => line.trim());
        const coordinates = [];
    
        for (const line of lines) {
            let coord = null;
            
            switch (format) {
                case 'decimal':
                    coord = this.parseDecimalDegrees(line);
                    break;
                case 'dms':
                    coord = this.parseDMS(line);
                    break;
                case 'geojson':
                    coord = this.parseGeoJSONCoordinate(line);
                    break;
            }
    
            if (coord) {
                coordinates.push(coord);
            } else {
                throw new Error(`Invalid coordinate: ${line}`);
            }
        }
    
        // Handle different geometry types
        if (geometryType === 'point') {
            if (coordinates.length !== 1) {
                throw new Error('Point geometry requires exactly one coordinate');
            }
            return coordinates[0]; // Return single coordinate for point
        } else if (geometryType === 'multipoint') {
            return coordinates; // Return array of coordinates for multipoint
        } else {
            // Polygon - close if not closed
            if (coordinates.length > 2) {
                const first = coordinates[0];
                const last = coordinates[coordinates.length - 1];
                if (first[0] !== last[0] || first[1] !== last[1]) {
                    coordinates.push([first[0], first[1]]);
                }
            }
            return coordinates;
        }
    }

    parseDecimalDegrees(line) {
        // Parse formats like: "-73.778925, 40.639751" or "-73.778925 40.639751"
        const match = line.match(/(-?\d+\.?\d*)[,\s]+(-?\d+\.?\d*)/);
        if (match) {
            return [parseFloat(match[1]), parseFloat(match[2])];
        }
        return null;
    }

    parseDMS(line) {
        // Parse formats like: "40°38'23.1"N 73°46'44.1"W"
        const dmsRegex = /(\d+)°(\d+)'([\d.]+)"([NS])\s+(\d+)°(\d+)'([\d.]+)"([EW])/;
        const match = line.match(dmsRegex);
        if (match) {
            let lat = parseInt(match[1]) + parseInt(match[2])/60 + parseFloat(match[3])/3600;
            let lng = parseInt(match[5]) + parseInt(match[6])/60 + parseFloat(match[7])/3600;
            
            if (match[4] === 'S') lat = -lat;
            if (match[8] === 'W') lng = -lng;
            
            return [lng, lat];
        }
        return null;
    }

    parseGeoJSONCoordinate(line) {
        // Parse JSON array format: "[longitude, latitude]"
        try {
            const coord = JSON.parse(line);
            if (Array.isArray(coord) && coord.length >= 2) {
                return [coord[0], coord[1]];
            }
        } catch (e) {
            // Invalid JSON
        }
        return null;
    }

    validateAndPreview() {
        try {
            const format = document.getElementById('coordinate-format').value;
            const coordText = document.getElementById('coordinates-input').value;
            const geometryType = document.getElementById('geometry-type-selector').value;
            
            if (!coordText.trim()) {
                showNotification('Please enter coordinates', 'warning');
                return;
            }
    
            const coordinates = this.parseCoordinates(coordText, format, geometryType);
            
            let geojson;
            
            if (geometryType === 'point') {
                geojson = {
                    type: 'Feature',
                    properties: {},
                    geometry: {
                        type: 'Point',
                        coordinates: coordinates
                    }
                };
            } else if (geometryType === 'multipoint') {
                geojson = {
                    type: 'Feature',
                    properties: {},
                    geometry: {
                        type: 'MultiPoint',
                        coordinates: coordinates
                    }
                };
            } else {
                if (coordinates.length < 3) {
                    showNotification('A polygon requires at least 3 points', 'warning');
                    return;
                }
                geojson = {
                    type: 'Feature',
                    properties: {},
                    geometry: {
                        type: 'Polygon',
                        coordinates: [coordinates]
                    }
                };
            }
    
            // Show preview
            this.showPreview(geojson);
            this.isValid = true;
            
            // Show/hide point options
            const pointOptionsGroup = document.getElementById('point-options-group');
            if (geometryType === 'point' || geometryType === 'multipoint') {
                pointOptionsGroup.classList.remove('hidden');
            } else {
                pointOptionsGroup.classList.add('hidden');
            }
            
            // Enable submit button
            document.querySelector('#coordinate-entry-form button[type="submit"]').disabled = false;
            
            showNotification('Coordinates validated successfully', 'success');
            
        } catch (error) {
            showNotification(`Validation failed: ${error.message}`, 'error');
            this.isValid = false;
        }
    }

    showPreview(geojson) {
        const previewContainer = document.getElementById('coordinate-preview-container');
        previewContainer.classList.remove('hidden');
    
        // Clear previous preview
        if (this.previewLayer) {
            this.previewMap.removeLayer(this.previewLayer);
        }
    
        // Handle different geometry types
        if (geojson.geometry.type === 'Point') {
            const coords = geojson.geometry.coordinates;
            this.previewLayer = L.marker([coords[1], coords[0]]).addTo(this.previewMap);
            
            // Also show the operation radius if specified
            const radius = parseInt(document.getElementById('point-operation-radius')?.value) || 100;
            L.circle([coords[1], coords[0]], {
                radius: radius,
                color: '#3b82f6',
                fillOpacity: 0.2
            }).addTo(this.previewMap);
            
            this.previewMap.setView([coords[1], coords[0]], 15);
            
        } else if (geojson.geometry.type === 'MultiPoint') {
            const markers = [];
            geojson.geometry.coordinates.forEach(coords => {
                const marker = L.marker([coords[1], coords[0]]);
                markers.push(marker);
                
                // Show operation radius for each point
                const radius = parseInt(document.getElementById('point-operation-radius')?.value) || 100;
                L.circle([coords[1], coords[0]], {
                    radius: radius,
                    color: '#3b82f6',
                    fillOpacity: 0.2
                }).addTo(this.previewMap);
            });
            
            this.previewLayer = L.layerGroup(markers).addTo(this.previewMap);
            const group = new L.featureGroup(markers);
            this.previewMap.fitBounds(group.getBounds().pad(0.1));
            
        } else {
            // Polygon
            this.previewLayer = L.geoJSON(geojson, {
                style: {
                    color: '#3b82f6',
                    weight: 2,
                    fillOpacity: 0.3
                }
            }).addTo(this.previewMap);
            this.previewMap.fitBounds(this.previewLayer.getBounds());
        }
    }

    clearPreview() {
        if (this.previewLayer && this.previewMap) {
            this.previewMap.removeLayer(this.previewLayer);
            this.previewLayer = null;
        }
        
        const previewContainer = document.getElementById('coordinate-preview-container');
        if (previewContainer) {
            previewContainer.classList.add('hidden');
        }
    }

    createZoneFromCoordinates() {
        if (!this.isValid) {
            showNotification('Please validate coordinates first', 'warning');
            return;
        }

        try {
            const format = document.getElementById('coordinate-format').value;
            const coordText = document.getElementById('coordinates-input').value;
            const zoneName = document.getElementById('zone-name').value;
            const altitudeMin = parseInt(document.getElementById('coord-altitude-min').value);
            const altitudeMax = parseInt(document.getElementById('coord-altitude-max').value);
            
            const coordinates = this.parseCoordinates(coordText, format);
            
            // Create GeoJSON feature
            const feature = {
                type: 'Feature',
                properties: {
                    name: zoneName,
                    shapeType: 'polygon'
                },
                geometry: {
                    type: 'Polygon',
                    coordinates: [coordinates]
                }
            };

            // Create form data for drone zone
            const formData = {
                operationId: zoneName || `ZONE-${Date.now()}`,
                altitudeMin: altitudeMin,
                altitudeMax: altitudeMax,
                startTime: new Date().toISOString(),
                endTime: new Date(Date.now() + 86400000).toISOString(), // +24 hours
                status: 'Planned'
            };

            // Add to map using existing drone zone creation
            if (window.mapManager) {
                window.mapManager.createDroneZone(feature, formData);
            }

            this.hideModal();
            showNotification('Drone zone created successfully', 'success');
            
        } catch (error) {
            showNotification(`Failed to create zone: ${error.message}`, 'error');
        }
    }

    updatePlaceholder(format) {
        const textarea = document.getElementById('coordinates-input');
        
        switch (format) {
            case 'decimal':
                textarea.placeholder = 'Example:\n-73.778925, 40.639751\n-73.750000, 40.680000\n-73.700000, 40.750000';
                break;
            case 'dms':
                textarea.placeholder = 'Example:\n40°38\'23.1"N 73°46\'44.1"W\n40°40\'48.0"N 73°45\'0.0"W';
                break;
            case 'geojson':
                textarea.placeholder = 'Example:\n[-73.778925, 40.639751]\n[-73.750000, 40.680000]\n[-73.700000, 40.750000]';
                break;
        }
    }
}

export default new CoordinateEntryManager();