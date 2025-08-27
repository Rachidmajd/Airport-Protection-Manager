// Enhanced frontend/js/coordinateEntry.js with circle support
import mapManager from './map.js';

class CoordinateEntryManager {
    constructor() {
        this.modal = null;
        this.previewMap = null;
        this.previewLayer = null;
        this.isValid = false;
    }

    // Utility method to show notifications with fallback
    showNotification(message, type = 'info') {
        console.log(`📢 Notification (${type}):`, message);
        
        // Try to use the global showNotification function
        if (window.showNotification && typeof window.showNotification === 'function') {
            window.showNotification(message, type);
        } 
        // Try to use uiManager if available
        else if (window.uiManager && window.uiManager.showNotification) {
            window.uiManager.showNotification(message, type);
        }
        // Fallback to alert for critical messages
        else if (type === 'error' || type === 'warning') {
            alert(message);
        } else {
            console.log('💡 Notification fallback:', message);
        }
    }

    init() {
        this.modal = document.getElementById('coordinate-entry-modal');
        this.setupEventHandlers();
    }

    setupEventHandlers() {
        console.log('🔧 Setting up coordinate entry event handlers...');
        
        // Use a more direct approach - wait for DOM to be ready
        setTimeout(() => {
            this.attachEventHandlers();
        }, 100);
    }

    attachEventHandlers() {
        console.log('🔗 Attaching event handlers to elements...');
        
        // Open modal button
        const manualBtn = document.getElementById('manual-entry-btn');
        if (manualBtn) {
            manualBtn.onclick = () => {
                console.log('🖱️ Manual entry button clicked');
                this.showModal();
            };
            console.log('✅ Manual entry button handler set');
        } else {
            console.warn('⚠️ Manual entry button not found');
        }

        // Close modal buttons
        const closeBtn = document.getElementById('close-coordinate-modal');
        const cancelBtn = document.getElementById('cancel-coordinate-entry');
        
        if (closeBtn) {
            closeBtn.onclick = () => {
                console.log('🖱️ Close button clicked');
                this.hideModal();
            };
        }
        
        if (cancelBtn) {
            cancelBtn.onclick = () => {
                console.log('🖱️ Cancel button clicked');
                this.hideModal();
            };
        }

        // VALIDATE BUTTON - Use direct onclick assignment
        const validateBtn = document.getElementById('validate-coordinates');
        if (validateBtn) {
            console.log('🔧 Setting up validate button handler...');
            validateBtn.onclick = (e) => {
                console.log('🖱️ VALIDATE BUTTON CLICKED! Event:', e);
                e.preventDefault();
                e.stopPropagation();
                
                // Add visual feedback
                validateBtn.style.transform = 'scale(0.95)';
                setTimeout(() => {
                    validateBtn.style.transform = 'scale(1)';
                }, 150);
                
                console.log('🔄 Calling validateAndPreview...');
                this.validateAndPreview();
            };
            
            // Also try addEventListener as backup
            validateBtn.addEventListener('click', (e) => {
                console.log('🖱️ Validate button addEventListener fired');
            });
            
            console.log('✅ Validate button handlers set');
            console.log('Button element:', validateBtn);
            console.log('Button onclick:', validateBtn.onclick);
        } else {
            console.error('❌ Validate button not found!');
        }

        // FORM SUBMISSION - Use direct onsubmit
        const form = document.getElementById('coordinate-entry-form');
        if (form) {
            console.log('🔧 Setting up form submission handler...');
            form.onsubmit = (e) => {
                console.log('📝 FORM SUBMIT TRIGGERED!');
                e.preventDefault();
                e.stopPropagation();
                this.createZoneFromCoordinates();
                return false;
            };
            
            console.log('✅ Form submit handler set');
            console.log('Form element:', form);
            console.log('Form onsubmit:', form.onsubmit);
        } else {
            console.error('❌ Form not found!');
        }

        // SUBMIT BUTTON - Direct onclick
        const submitBtn = document.querySelector('#coordinate-entry-form button[type="submit"]');
        if (submitBtn) {
            console.log('🔧 Setting up submit button handler...');
            submitBtn.onclick = (e) => {
                console.log('🖱️ SUBMIT BUTTON CLICKED!');
                console.log('Button disabled:', e.target.disabled);
                console.log('Form valid:', this.isValid);
                
                if (e.target.disabled) {
                    console.warn('⚠️ Submit button is disabled');
                    e.preventDefault();
                    e.stopPropagation();
                    this.showNotification('Please validate coordinates first', 'warning');
                    return false;
                }
                
                console.log('🔄 Calling createZoneFromCoordinates directly...');
                e.preventDefault();
                e.stopPropagation();
                this.createZoneFromCoordinates();
                return false;
            };
            
            console.log('✅ Submit button handler set');
        }

        // Format and geometry type changes - direct onchange
        const formatSelect = document.getElementById('coordinate-format');
        if (formatSelect) {
            formatSelect.onchange = (e) => {
                console.log('🖱️ Format changed to:', e.target.value);
                this.updatePlaceholder(e.target.value);
            };
        }

        const geometrySelect = document.getElementById('geometry-type-selector');
        if (geometrySelect) {
            geometrySelect.onchange = (e) => {
                console.log('🖱️ Geometry type changed to:', e.target.value);
                this.handleGeometryTypeChange(e.target.value);
            };
        }
        
        console.log('✅ All event handlers attached using direct assignment');
    }

    showModal() {
        if (this.modal) {
            this.modal.classList.remove('hidden');
            this.initPreviewMap();
            this.handleGeometryTypeChange(document.getElementById('geometry-type-selector').value);
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

    handleGeometryTypeChange(geometryType) {
        const pointOptionsGroup = document.getElementById('point-options-group');
        const coordinatesLabel = document.querySelector('label[for="coordinates-input"]');
        const coordinatesInput = document.getElementById('coordinates-input');
        const circleInfoPanel = document.getElementById('circle-info-panel');
        const circleOnlyElements = document.querySelectorAll('.circle-only');
        
        // Show/hide geometry-specific options
        if (geometryType === 'point' || geometryType === 'multipoint') {
            pointOptionsGroup?.classList.remove('hidden');
        } else {
            pointOptionsGroup?.classList.add('hidden');
        }

        // Show/hide circle-specific elements
        if (geometryType === 'circle') {
            circleOnlyElements.forEach(el => el.classList.remove('hidden'));
            circleInfoPanel?.classList.remove('hidden');
        } else {
            circleOnlyElements.forEach(el => el.classList.add('hidden'));
            circleInfoPanel?.classList.add('hidden');
        }

        // Update labels and placeholders based on geometry type
        if (coordinatesLabel && coordinatesInput) {
            switch (geometryType) {
                case 'circle':
                    coordinatesLabel.innerHTML = `
                        Circle Center & Radius *
                        <small>Enter center coordinate and radius in meters</small>
                    `;
                    this.updatePlaceholderForCircle();
                    break;
                case 'point':
                    coordinatesLabel.innerHTML = `
                        Point Coordinate *
                        <small>Enter one coordinate (longitude, latitude)</small>
                    `;
                    this.updatePlaceholder(document.getElementById('coordinate-format').value);
                    break;
                case 'multipoint':
                    coordinatesLabel.innerHTML = `
                        Multiple Points *
                        <small>Enter one coordinate per line (longitude, latitude)</small>
                    `;
                    this.updatePlaceholder(document.getElementById('coordinate-format').value);
                    break;
                default:
                    coordinatesLabel.innerHTML = `
                        Polygon Coordinates *
                        <small>Enter one coordinate per line (longitude, latitude)</small>
                    `;
                    this.updatePlaceholder(document.getElementById('coordinate-format').value);
            }
        }

        // Clear previous validation and reset form state
        this.isValid = false;
        const submitBtn = document.querySelector('#coordinate-entry-form button[type="submit"]');
        if (submitBtn) submitBtn.disabled = true;
        this.clearPreview();
        
        // Clear any validation messages
        this.clearValidationMessages();
    }

    parseCoordinates(text, format, geometryType) {
        const lines = text.trim().split('\n').filter(line => line.trim());
        
        if (geometryType === 'circle') {
            return this.parseCircleCoordinates(lines, format);
        }
        
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
            return coordinates[0];
        } else if (geometryType === 'multipoint') {
            return coordinates;
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

    parseCircleCoordinates(lines, format) {
        if (lines.length !== 1) {
            throw new Error('Circle geometry requires exactly one line with center coordinate and radius');
        }
        
        const line = lines[0];
        let centerCoord = null;
        let radius = null;
        
        console.log(`🔍 Parsing circle coordinates in ${format} format:`, line);
        
        switch (format) {
            case 'decimal':
                // Format: "longitude, latitude, radius" or "longitude latitude radius"
                const decimalMatch = line.match(/(-?\d+\.?\d*)[,\s]+(-?\d+\.?\d*)[,\s]+(\d+\.?\d*)/);
                if (decimalMatch) {
                    centerCoord = [parseFloat(decimalMatch[1]), parseFloat(decimalMatch[2])];
                    radius = parseFloat(decimalMatch[3]);
                    console.log('✅ Decimal parsed:', { centerCoord, radius });
                }
                break;
                
            case 'dms':
                // Enhanced DMS regex to handle various formats with flexible spacing
                // Supports: 31°34'21.1"N 8°00'20.5"W 500
                const dmsPattern = /(\d+)°(\d+)'([\d.]+)"([NS])\s+(\d+)°(\d+)'([\d.]+)"([EW])\s+(\d+\.?\d*)/;
                const dmsMatch = line.match(dmsPattern);
                
                console.log('🔍 DMS regex test:', dmsPattern.test(line));
                console.log('🔍 DMS match result:', dmsMatch);
                
                if (dmsMatch) {
                    // Parse latitude (first coordinate group)
                    let lat = parseInt(dmsMatch[1]) + parseInt(dmsMatch[2])/60 + parseFloat(dmsMatch[3])/3600;
                    let lng = parseInt(dmsMatch[5]) + parseInt(dmsMatch[6])/60 + parseFloat(dmsMatch[7])/3600;
                    
                    // Apply hemisphere corrections
                    if (dmsMatch[4] === 'S') lat = -lat;
                    if (dmsMatch[8] === 'W') lng = -lng;
                    
                    centerCoord = [lng, lat]; // GeoJSON format: [longitude, latitude]
                    radius = parseFloat(dmsMatch[9]);
                    
                    console.log('✅ DMS parsed:', { 
                        originalLat: `${dmsMatch[1]}°${dmsMatch[2]}'${dmsMatch[3]}"${dmsMatch[4]}`,
                        originalLng: `${dmsMatch[5]}°${dmsMatch[6]}'${dmsMatch[7]}"${dmsMatch[8]}`,
                        decimalLat: lat,
                        decimalLng: lng,
                        centerCoord, 
                        radius 
                    });
                } else {
                    console.error('❌ DMS parsing failed for:', line);
                    console.log('💡 Expected format: 31°34\'21.1"N 8°00\'20.5"W 500');
                    console.log('💡 Your format:', line);
                }
                break;
                
            case 'geojson':
                // Format: "[-73.778925, 40.639751, 500]"
                try {
                    const parsed = JSON.parse(line);
                    if (Array.isArray(parsed) && parsed.length >= 3) {
                        centerCoord = [parsed[0], parsed[1]];
                        radius = parsed[2];
                        console.log('✅ GeoJSON parsed:', { centerCoord, radius });
                    }
                } catch (e) {
                    console.error('❌ GeoJSON parsing failed:', e);
                }
                break;
        }
        
        if (!centerCoord || radius === null) {
            const formatExamples = {
                decimal: "longitude, latitude, radius (e.g., -8.005694, 31.572528, 500)",
                dms: 'degrees°minutes\'seconds"hemisphere (e.g., 31°34\'21.1"N 8°00\'20.5"W 500)',
                geojson: "[longitude, latitude, radius] (e.g., [-8.005694, 31.572528, 500])"
            };
            
            throw new Error(`Invalid circle format for ${format}. Expected: ${formatExamples[format]}. Got: "${line}"`);
        }
        
        if (radius <= 0 || radius > 50000) {
            throw new Error(`Radius must be between 1 and 50,000 meters. Got: ${radius}m`);
        }
        
        // Validate coordinates are within valid ranges
        if (Math.abs(centerCoord[1]) > 90) {
            throw new Error(`Latitude must be between -90 and 90 degrees. Got: ${centerCoord[1]}°`);
        }
        
        if (Math.abs(centerCoord[0]) > 180) {
            throw new Error(`Longitude must be between -180 and 180 degrees. Got: ${centerCoord[0]}°`);
        }
        
        console.log('✅ Circle coordinates validated successfully');
        return { center: centerCoord, radius: radius };
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

    createCirclePolygon(center, radiusMeters, numPoints = null) {
        // Get precision from UI if not specified
        if (!numPoints) {
            const circlePointsSelect = document.getElementById('circle-points');
            numPoints = circlePointsSelect ? parseInt(circlePointsSelect.value) : 64;
        }
        
        const points = [];
        const [centerLng, centerLat] = center;
        
        // Validate inputs
        if (Math.abs(centerLat) > 90) {
            throw new Error(`Invalid latitude: ${centerLat}. Must be between -90 and 90.`);
        }
        if (Math.abs(centerLng) > 180) {
            throw new Error(`Invalid longitude: ${centerLng}. Must be between -180 and 180.`);
        }
        
        // Convert radius from meters to degrees using more accurate calculation
        const metersPerDegreeLat = 111320; // Constant for latitude
        const metersPerDegreeLng = 111320 * Math.cos(centerLat * Math.PI / 180); // Varies by latitude
        
        const latRadiusDeg = radiusMeters / metersPerDegreeLat;
        const lngRadiusDeg = radiusMeters / metersPerDegreeLng;
        
        for (let i = 0; i < numPoints; i++) {
            const angle = (i * 360) / numPoints;
            const radians = (angle * Math.PI) / 180;
            
            const lat = centerLat + latRadiusDeg * Math.cos(radians);
            const lng = centerLng + lngRadiusDeg * Math.sin(radians);
            
            // Validate generated coordinates
            if (Math.abs(lat) > 90 || Math.abs(lng) > 180) {
                console.warn(`Generated coordinate out of bounds: ${lng}, ${lat} for radius ${radiusMeters}m`);
                // Continue with clamping
            }
            
            points.push([lng, lat]);
        }
        
        // Close the polygon
        points.push(points[0]);
        
        return points;
    }

    validateAndPreview() {
        console.log('🔍 Starting validation and preview...');
        
        try {
            this.clearValidationMessages();
            
            const format = document.getElementById('coordinate-format').value;
            const coordText = document.getElementById('coordinates-input').value;
            const geometryType = document.getElementById('geometry-type-selector').value;
            
            console.log('📋 Validation inputs:', { format, geometryType, coordText });
            
            if (!coordText.trim()) {
                console.warn('⚠️ No coordinates entered');
                this.showValidationMessage('Please enter coordinates', false);
                this.showNotification('Please enter coordinates', 'warning');
                return;
            }

            console.log('🔄 Starting coordinate parsing...');
            let geojson;
            
            if (geometryType === 'circle') {
                console.log('🎯 Processing circle geometry...');
                const circleData = this.parseCoordinates(coordText, format, geometryType);
                console.log('✅ Circle data parsed:', circleData);
                
                console.log('🔄 Creating circle polygon...');
                const polygonCoords = this.createCirclePolygon(circleData.center, circleData.radius);
                console.log('✅ Circle polygon created with', polygonCoords.length - 1, 'points');
                
                geojson = {
                    type: 'Feature',
                    properties: {
                        shapeType: 'circle',
                        center: circleData.center,
                        radius: circleData.radius,
                        isCircle: true,
                        area: Math.PI * circleData.radius * circleData.radius,
                        circumference: 2 * Math.PI * circleData.radius
                    },
                    geometry: {
                        type: 'Polygon',
                        coordinates: [polygonCoords]
                    }
                };
                
                console.log('✅ Circle GeoJSON created:', geojson);
                
                // Show circle-specific info
                this.showCircleInfo(circleData);
                this.showValidationMessage(`✅ Circle validated: ${circleData.radius}m radius, ${(Math.PI * circleData.radius * circleData.radius / 1000000).toFixed(3)} km² area`, true);
                
            } else if (geometryType === 'point') {
                console.log('📍 Processing point geometry...');
                const coordinates = this.parseCoordinates(coordText, format, geometryType);
                
                geojson = {
                    type: 'Feature',
                    properties: { shapeType: 'point', isPoint: true },
                    geometry: {
                        type: 'Point',
                        coordinates: coordinates
                    }
                };
                this.showValidationMessage('✅ Point coordinates validated successfully', true);
                
            } else if (geometryType === 'multipoint') {
                console.log('📍 Processing multipoint geometry...');
                const coordinates = this.parseCoordinates(coordText, format, geometryType);
                
                geojson = {
                    type: 'Feature',
                    properties: { shapeType: 'multipoint' },
                    geometry: {
                        type: 'MultiPoint',
                        coordinates: coordinates
                    }
                };
                this.showValidationMessage(`✅ ${coordinates.length} points validated successfully`, true);
                
            } else {
                console.log('📐 Processing polygon geometry...');
                const coordinates = this.parseCoordinates(coordText, format, geometryType);
                
                if (coordinates.length < 3) {
                    this.showValidationMessage('A polygon requires at least 3 points', false);
                    this.showNotification('A polygon requires at least 3 points', 'warning');
                    return;
                }
                
                geojson = {
                    type: 'Feature',
                    properties: { shapeType: 'polygon' },
                    geometry: {
                        type: 'Polygon',
                        coordinates: [coordinates]
                    }
                };
                this.showValidationMessage(`✅ Polygon validated: ${coordinates.length - 1} vertices`, true);
            }

            console.log('🗺️ Showing preview...');
            // Show preview
            this.showPreview(geojson);
            this.isValid = true;
            
            console.log('✅ Validation successful, isValid set to:', this.isValid);
            
            // Show/hide geometry-specific options
            const pointOptionsGroup = document.getElementById('point-options-group');
            if (geometryType === 'point' || geometryType === 'multipoint') {
                pointOptionsGroup?.classList.remove('hidden');
            } else {
                pointOptionsGroup?.classList.add('hidden');
            }
            
            // Enable submit button with enhanced feedback
            const submitBtn = document.querySelector('#coordinate-entry-form button[type="submit"]');
            if (submitBtn) {
                console.log('🔄 Enabling submit button...');
                submitBtn.disabled = false;
                submitBtn.style.background = '#059669';
                submitBtn.style.cursor = 'pointer';
                submitBtn.textContent = `Create ${geometryType.charAt(0).toUpperCase() + geometryType.slice(1)} Zone`;
                
                console.log('✅ Submit button enabled:', {
                    disabled: submitBtn.disabled,
                    text: submitBtn.textContent,
                    style: submitBtn.style.background
                });
                
                // Add visual feedback
                submitBtn.style.transition = 'all 0.3s ease';
                submitBtn.style.transform = 'scale(1.02)';
                setTimeout(() => {
                    if (submitBtn) submitBtn.style.transform = 'scale(1)';
                }, 200);
                
            } else {
                console.error('❌ Submit button not found!');
            }
            
            console.log('🎉 Validation and preview complete');
            
        } catch (error) {
            console.error('❌ Validation error:', error);
            console.error('Error stack:', error.stack);
            this.showValidationMessage(`❌ ${error.message}`, false);
            this.showNotification(`Validation failed: ${error.message}`, 'error');
            this.isValid = false;
            
            // Disable submit button
            const submitBtn = document.querySelector('#coordinate-entry-form button[type="submit"]');
            if (submitBtn) {
                console.log('🔄 Disabling submit button due to error...');
                submitBtn.disabled = true;
                submitBtn.style.background = '';
                submitBtn.style.cursor = 'not-allowed';
                submitBtn.textContent = 'Create Zone';
            }
        }
    }

    showCircleInfo(circleData) {
        const container = document.getElementById('coordinate-preview-container');
        
        // Add circle info display
        let infoDiv = container.querySelector('.circle-info');
        if (!infoDiv) {
            infoDiv = document.createElement('div');
            infoDiv.className = 'circle-info';
            infoDiv.style.cssText = `
                background: linear-gradient(135deg, #f0f9ff, #e0f2fe);
                border: 1px solid #0ea5e9;
                border-radius: 8px;
                padding: 15px;
                margin-bottom: 15px;
                font-size: 13px;
                box-shadow: 0 2px 4px rgba(14, 165, 233, 0.1);
            `;
            container.insertBefore(infoDiv, container.firstChild);
        }
        
        // Calculate additional metrics
        const circumference = (2 * Math.PI * circleData.radius);
        const area = (Math.PI * circleData.radius * circleData.radius);
        const diameter = circleData.radius * 2;
        
        // Format coordinates nicely
        const latFormatted = Math.abs(circleData.center[1]).toFixed(6) + '°' + (circleData.center[1] >= 0 ? 'N' : 'S');
        const lngFormatted = Math.abs(circleData.center[0]).toFixed(6) + '°' + (circleData.center[0] >= 0 ? 'E' : 'W');
        
        infoDiv.innerHTML = `
            <div style="display: flex; align-items: center; gap: 8px; font-weight: 600; color: #0369a1; margin-bottom: 12px;">
                <span style="font-size: 16px;">🎯</span>
                <span>Circle Information</span>
            </div>
            
            <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 12px;">
                <div>
                    <div style="font-weight: 600; color: #374151; margin-bottom: 4px;">📍 Center</div>
                    <div style="font-family: monospace; font-size: 12px;">${latFormatted}</div>
                    <div style="font-family: monospace; font-size: 12px;">${lngFormatted}</div>
                </div>
                <div>
                    <div style="font-weight: 600; color: #374151; margin-bottom: 4px;">📏 Dimensions</div>
                    <div><strong>Radius:</strong> ${circleData.radius.toLocaleString()}m</div>
                    <div><strong>Diameter:</strong> ${diameter.toLocaleString()}m</div>
                </div>
            </div>
            
            <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 12px;">
                <div>
                    <div style="font-weight: 600; color: #374151; margin-bottom: 4px;">📐 Measurements</div>
                    <div><strong>Circumference:</strong> ${(circumference / 1000).toFixed(2)} km</div>
                    <div><strong>Area:</strong> ${(area / 1000000).toFixed(3)} km²</div>
                </div>
                <div>
                    <div style="font-weight: 600; color: #374151; margin-bottom: 4px;">🚁 Aviation Units</div>
                    <div><strong>Radius:</strong> ${(circleData.radius * 3.28084).toLocaleString()} ft</div>
                    <div><strong>Area:</strong> ${(area * 0.000247105).toFixed(2)} acres</div>
                </div>
            </div>
            
            <div style="margin-top: 12px; padding-top: 12px; border-top: 1px solid #bfdbfe; font-size: 11px; color: #6b7280;">
                💡 This circle will be converted to a ${document.getElementById('circle-points')?.value || 64}-sided polygon for mapping
            </div>
        `;
    }

    clearValidationMessages() {
        // Remove any existing validation message elements
        const existingMessages = document.querySelectorAll('.validation-success, .validation-error');
        existingMessages.forEach(msg => msg.remove());
    }

    showValidationMessage(message, isSuccess = true) {
        this.clearValidationMessages();
        
        const messageDiv = document.createElement('div');
        messageDiv.className = isSuccess ? 'validation-success' : 'validation-error';
        messageDiv.textContent = message;
        
        const coordinatesInput = document.getElementById('coordinates-input');
        coordinatesInput.parentNode.appendChild(messageDiv);
        
        // Auto-remove error messages after 5 seconds
        if (!isSuccess) {
            setTimeout(() => {
                if (messageDiv.parentNode) {
                    messageDiv.remove();
                }
            }, 5000);
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
            
            // Show operation radius if specified
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
            // Polygon (including circles converted to polygons)
            const style = {
                color: geojson.properties.isCircle ? '#10b981' : '#3b82f6',
                weight: 2,
                fillOpacity: 0.3
            };
            
            this.previewLayer = L.geoJSON(geojson, { style }).addTo(this.previewMap);
            this.previewMap.fitBounds(this.previewLayer.getBounds());
            
            // For circles, also show the center point
            if (geojson.properties.isCircle && geojson.properties.center) {
                const center = geojson.properties.center;
                L.marker([center[1], center[0]], {
                    icon: L.divIcon({
                        className: 'circle-center-marker',
                        html: '<div style="background: #10b981; width: 8px; height: 8px; border-radius: 50%; border: 2px solid white;"></div>',
                        iconSize: [12, 12],
                        iconAnchor: [6, 6]
                    })
                }).addTo(this.previewMap);
            }
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
            // Remove circle info if present
            const circleInfo = previewContainer.querySelector('.circle-info');
            if (circleInfo) {
                circleInfo.remove();
            }
        }
    }

    createZoneFromCoordinates() {
        console.log('🚀 === STARTING ZONE CREATION ===');
        
        if (!this.isValid) {
            console.warn('⚠️ Coordinates not validated');
            this.showValidationMessage('Please validate coordinates first', false);
            this.showNotification('Please validate coordinates first', 'warning');
            return;
        }

        try {
            console.log('✅ Validation state confirmed: isValid =', this.isValid);
            
            // Get all form values with detailed logging
            const format = document.getElementById('coordinate-format').value;
            const coordText = document.getElementById('coordinates-input').value;
            const geometryType = document.getElementById('geometry-type-selector').value;
            const zoneName = document.getElementById('zone-name').value;
            const altitudeMin = parseInt(document.getElementById('coord-altitude-min').value);
            const altitudeMax = parseInt(document.getElementById('coord-altitude-max').value);
            
            console.log('📋 Form data collected:', { 
                format, 
                geometryType, 
                zoneName, 
                altitudeMin, 
                altitudeMax,
                coordTextLength: coordText?.length
            });
            
            // Validate required fields
            if (!zoneName.trim()) {
                console.error('❌ Zone name is empty');
                this.showValidationMessage('Zone name is required', false);
                this.showNotification('Zone name is required', 'warning');
                return;
            }

            if (isNaN(altitudeMin) || isNaN(altitudeMax)) {
                console.error('❌ Invalid altitude values:', { altitudeMin, altitudeMax });
                this.showValidationMessage('Valid altitude values are required', false);
                this.showNotification('Valid altitude values are required', 'warning');
                return;
            }

            if (altitudeMin >= altitudeMax) {
                console.error('❌ Altitude range invalid:', { altitudeMin, altitudeMax });
                this.showValidationMessage('Maximum altitude must be greater than minimum altitude', false);
                this.showNotification('Maximum altitude must be greater than minimum altitude', 'warning');
                return;
            }
            
            console.log('✅ Form validation passed');
            
            let feature;
            let shapeDescription = '';
            
            if (geometryType === 'circle') {
                console.log('🎯 === CREATING CIRCLE GEOMETRY ===');
                
                console.log('🔄 Step 1: Parsing circle coordinates...');
                const circleData = this.parseCoordinates(coordText, format, geometryType);
                console.log('✅ Circle data parsed:', circleData);
                
                console.log('🔄 Step 2: Creating circle polygon...');
                const polygonCoords = this.createCirclePolygon(circleData.center, circleData.radius);
                console.log('✅ Circle polygon created:', {
                    pointCount: polygonCoords.length,
                    firstPoint: polygonCoords[0],
                    lastPoint: polygonCoords[polygonCoords.length - 1]
                });
                
                console.log('🔄 Step 3: Building GeoJSON feature...');
                feature = {
                    type: 'Feature',
                    properties: {
                        name: zoneName,
                        shapeType: 'circle',
                        center: circleData.center,
                        radius: circleData.radius,
                        isCircle: true
                    },
                    geometry: {
                        type: 'Polygon',
                        coordinates: [polygonCoords]
                    }
                };
                shapeDescription = `circular zone (${circleData.radius}m radius)`;
                console.log('✅ Circle feature created:', feature);
                
            } else if (geometryType === 'point') {
                console.log('📍 Creating point geometry...');
                const coordinates = this.parseCoordinates(coordText, format, geometryType);
                
                feature = {
                    type: 'Feature',
                    properties: {
                        name: zoneName,
                        shapeType: 'point',
                        isPoint: true
                    },
                    geometry: {
                        type: 'Point',
                        coordinates: coordinates
                    }
                };
                shapeDescription = 'point zone';
                
            } else if (geometryType === 'multipoint') {
                console.log('📍 Creating multipoint geometry...');
                const coordinates = this.parseCoordinates(coordText, format, geometryType);
                
                feature = {
                    type: 'Feature',
                    properties: {
                        name: zoneName,
                        shapeType: 'multipoint'
                    },
                    geometry: {
                        type: 'MultiPoint',
                        coordinates: coordinates
                    }
                };
                shapeDescription = `multipoint zone (${coordinates.length} points)`;
                
            } else {
                console.log('📐 Creating polygon geometry...');
                const coordinates = this.parseCoordinates(coordText, format, geometryType);
                
                feature = {
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
                shapeDescription = `polygon zone (${coordinates.length - 1} vertices)`;
            }

            console.log('✅ Feature geometry created successfully:', feature.geometry.type);

            // Create form data for drone zone
            console.log('🔄 Step 4: Creating form data for map manager...');
            const formData = {
                operationId: zoneName || `ZONE-${Date.now()}`,
                altitudeMin: altitudeMin,
                altitudeMax: altitudeMax,
                startTime: new Date().toISOString(),
                endTime: new Date(Date.now() + 86400000).toISOString(), // +24 hours
                status: 'Planned'
            };

            // Add point radius if applicable
            if (geometryType === 'point' || geometryType === 'multipoint') {
                formData.pointRadius = parseInt(document.getElementById('point-operation-radius')?.value) || 100;
                console.log('📏 Added point radius:', formData.pointRadius);
            }

            console.log('✅ Form data prepared:', formData);

            // Check if map manager is available
            console.log('🔄 Step 5: Checking map manager availability...');
            if (!window.mapManager) {
                console.error('❌ Map manager not available on window object');
                console.log('Available window properties:', Object.keys(window).filter(k => k.includes('map')));
                throw new Error('Map manager not available. Please refresh the page.');
            }
            
            console.log('✅ Map manager found:', typeof window.mapManager);
            console.log('Map manager methods:', Object.getOwnPropertyNames(Object.getPrototypeOf(window.mapManager)));

            // Check if createDroneZone method exists
            if (typeof window.mapManager.createDroneZone !== 'function') {
                console.error('❌ createDroneZone method not found on map manager');
                console.log('Available methods:', Object.getOwnPropertyNames(window.mapManager));
                throw new Error('createDroneZone method not available on map manager');
            }

            // Add to map using existing drone zone creation
            console.log('🗺️ Step 6: Calling mapManager.createDroneZone...');
            console.log('Calling with:', { feature: feature.geometry.type, formData: formData.operationId });
            
            const result = window.mapManager.createDroneZone(feature, formData);
            console.log('🔄 createDroneZone returned:', result);
            
            if (result || result === undefined) { // Some functions don't return anything but still work
                console.log('✅ Zone creation call completed');
                
                // Hide modal
                console.log('🔄 Step 7: Hiding modal...');
                this.hideModal();
                
                // Show success message
                const successMessage = `Created ${shapeDescription} successfully`;
                console.log('🎉 SUCCESS:', successMessage);
                this.showNotification(successMessage, 'success');
                
                console.log('🎉 === ZONE CREATION COMPLETED SUCCESSFULLY ===');
            } else {
                console.warn('⚠️ Zone creation returned falsy result:', result);
                throw new Error('Map manager returned an error creating the zone');
            }
            
        } catch (error) {
            console.error('❌ === ZONE CREATION FAILED ===');
            console.error('Error message:', error.message);
            console.error('Error stack:', error.stack);
            
            const errorMessage = `Failed to create zone: ${error.message}`;
            this.showValidationMessage(errorMessage, false);
            this.showNotification(errorMessage, 'error');
        }
    }

    updatePlaceholder(format) {
        const textarea = document.getElementById('coordinates-input');
        const geometryType = document.getElementById('geometry-type-selector').value;
        
        if (geometryType === 'circle') {
            this.updatePlaceholderForCircle();
            return;
        }
        
        switch (format) {
            case 'decimal':
                if (geometryType === 'point') {
                    textarea.placeholder = 'Example:\n-73.778925, 40.639751';
                } else {
                    textarea.placeholder = 'Example:\n-73.778925, 40.639751\n-73.750000, 40.680000\n-73.700000, 40.750000';
                }
                break;
            case 'dms':
                if (geometryType === 'point') {
                    textarea.placeholder = 'Example:\n40°38\'23.1"N 73°46\'44.1"W';
                } else {
                    textarea.placeholder = 'Example:\n40°38\'23.1"N 73°46\'44.1"W\n40°40\'48.0"N 73°45\'0.0"W';
                }
                break;
            case 'geojson':
                if (geometryType === 'point') {
                    textarea.placeholder = 'Example:\n[-73.778925, 40.639751]';
                } else {
                    textarea.placeholder = 'Example:\n[-73.778925, 40.639751]\n[-73.750000, 40.680000]\n[-73.700000, 40.750000]';
                }
                break;
        }
    }

    updatePlaceholderForCircle() {
        const textarea = document.getElementById('coordinates-input');
        const format = document.getElementById('coordinate-format').value;
        
        switch (format) {
            case 'decimal':
                textarea.placeholder = 'Example:\n-8.005694, 31.572528, 500\n(longitude, latitude, radius_in_meters)';
                break;
            case 'dms':
                textarea.placeholder = 'Example:\n31°34\'21.1"N 8°00\'20.5"W 500\n(lat_DMS lng_DMS radius_in_meters)';
                break;
            case 'geojson':
                textarea.placeholder = 'Example:\n[-8.005694, 31.572528, 500]\n[longitude, latitude, radius_in_meters]';
                break;
        }
    }
    // Debug method to test coordinate entry
    debugCoordinateEntry() {
        console.log('🐛 Debugging coordinate entry state...');
        console.log('Modal element:', this.modal);
        console.log('Is valid:', this.isValid);
        console.log('Preview map:', !!this.previewMap);
        
        // Check form elements
        const form = document.getElementById('coordinate-entry-form');
        const validateBtn = document.getElementById('validate-coordinates');
        const submitBtn = document.querySelector('#coordinate-entry-form button[type="submit"]');
        const coordinatesInput = document.getElementById('coordinates-input');
        const zoneNameInput = document.getElementById('zone-name');
        
        console.log('Form element:', !!form);
        console.log('Validate button:', !!validateBtn);
        console.log('Submit button:', !!submitBtn);
        console.log('Submit button disabled:', submitBtn?.disabled);
        console.log('Coordinates input:', !!coordinatesInput);
        console.log('Zone name input:', !!zoneNameInput);
        console.log('Zone name value:', zoneNameInput?.value);
        console.log('Coordinates value length:', coordinatesInput?.value?.length);
        
        // Check if map manager is available
        console.log('Window mapManager:', !!window.mapManager);
        console.log('Notification functions:');
        console.log('  - window.showNotification:', typeof window.showNotification);
        console.log('  - window.uiManager.showNotification:', typeof window.uiManager?.showNotification);
        
        return {
            modal: !!this.modal,
            isValid: this.isValid,
            form: !!form,
            submitButton: !!submitBtn,
            submitDisabled: submitBtn?.disabled,
            mapManager: !!window.mapManager,
            coordinates: coordinatesInput?.value,
            zoneName: zoneNameInput?.value
        };
    }

    // Test DMS parsing specifically
    testDMSParsing(dmsString) {
        console.log('🧪 Testing DMS parsing for:', dmsString);
        try {
            const result = this.parseCircleCoordinates([dmsString], 'dms');
            console.log('✅ DMS parsing successful:', result);
            return result;
        } catch (error) {
            console.error('❌ DMS parsing failed:', error.message);
            return null;
        }
    }

    // Check button states and event handlers
    checkButtonStates() {
        console.log('🔍 Checking button states...');
        
        const validateBtn = document.getElementById('validate-coordinates');
        const submitBtn = document.querySelector('#coordinate-entry-form button[type="submit"]');
        const form = document.getElementById('coordinate-entry-form');
        
        const state = {
            validateButton: {
                exists: !!validateBtn,
                disabled: validateBtn?.disabled,
                visible: validateBtn?.style.display !== 'none',
                text: validateBtn?.textContent,
                hasEventListener: !!validateBtn?.onclick || validateBtn?._hasClickListener
            },
            submitButton: {
                exists: !!submitBtn,
                disabled: submitBtn?.disabled,
                visible: submitBtn?.style.display !== 'none',
                text: submitBtn?.textContent,
                background: submitBtn?.style.background
            },
            form: {
                exists: !!form,
                hasSubmitListener: !!form?.onsubmit || form?._hasSubmitListener
            },
            validationState: {
                isValid: this.isValid,
                modalVisible: !this.modal?.classList.contains('hidden')
            }
        };
        
        console.log('Button states:', state);
        
        // Test clicking the validate button programmatically
        if (validateBtn) {
            console.log('🧪 Testing validate button click...');
            try {
                validateBtn.click();
                console.log('✅ Validate button click successful');
            } catch (error) {
                console.error('❌ Validate button click failed:', error);
            }
        }
        
        return state;
    }

    // Force enable submit button for testing
    forceEnableSubmitButton() {
        console.log('🔧 Force enabling submit button...');
        const submitBtn = document.querySelector('#coordinate-entry-form button[type="submit"]');
        if (submitBtn) {
            submitBtn.disabled = false;
            submitBtn.style.background = '#059669';
            submitBtn.style.cursor = 'pointer';
            submitBtn.textContent = 'Create Zone (FORCED)';
            this.isValid = true;
            console.log('✅ Submit button force enabled');
        } else {
            console.error('❌ Submit button not found');
        }
    }

    // Manual trigger for validation - bypasses event system
    manualValidateAndPreview() {
        console.log('🔧 MANUAL VALIDATION TRIGGER');
        try {
            this.validateAndPreview();
            console.log('✅ Manual validation completed');
        } catch (error) {
            console.error('❌ Manual validation failed:', error);
        }
    }

    // Manual trigger for zone creation - bypasses event system
    manualCreateZone() {
        console.log('🔧 MANUAL ZONE CREATION TRIGGER');
        try {
            this.createZoneFromCoordinates();
            console.log('✅ Manual zone creation completed');
        } catch (error) {
            console.error('❌ Manual zone creation failed:', error);
        }
    }
}


// Make the coordinate entry manager available globally for debugging
const coordinateEntryManager = new CoordinateEntryManager();

// Add debug helpers to window
window.debugCoordinateEntry = () => coordinateEntryManager.debugCoordinateEntry();
window.testDMSParsing = (dmsString) => coordinateEntryManager.testDMSParsing(dmsString);
window.checkButtonStates = () => coordinateEntryManager.checkButtonStates();
window.forceEnableSubmitButton = () => coordinateEntryManager.forceEnableSubmitButton();
window.manualValidateAndPreview = () => coordinateEntryManager.manualValidateAndPreview();
window.manualCreateZone = () => coordinateEntryManager.manualCreateZone();

// Add a comprehensive test function
window.testCircleCreation = () => {
    console.log('🧪 Starting comprehensive circle creation test...');
    
    // Check if modal is open
    if (coordinateEntryManager.modal?.classList.contains('hidden')) {
        console.log('Opening modal...');
        coordinateEntryManager.showModal();
    }
    
    // Wait a bit for modal to open
    setTimeout(() => {
        // Set up test data
        const geometrySelect = document.getElementById('geometry-type-selector');
        const formatSelect = document.getElementById('coordinate-format');
        const coordinatesInput = document.getElementById('coordinates-input');
        const zoneNameInput = document.getElementById('zone-name');
        
        console.log('Setting form values...');
        if (geometrySelect) {
            geometrySelect.value = 'circle';
            console.log('✅ Geometry type set to circle');
        }
        if (formatSelect) {
            formatSelect.value = 'dms';
            console.log('✅ Format set to DMS');
        }
        if (coordinatesInput) {
            coordinatesInput.value = '31°34\'21.1"N 8°00\'20.5"W 500';
            console.log('✅ Coordinates set');
        }
        if (zoneNameInput) {
            zoneNameInput.value = 'Test Circle Zone';
            console.log('✅ Zone name set');
        }
        
        // Trigger geometry type change
        if (geometrySelect) {
            coordinateEntryManager.handleGeometryTypeChange('circle');
            console.log('✅ Geometry type change handled');
        }
        
        console.log('🎯 Test setup complete!');
        console.log('💡 Now try these commands:');
        console.log('   - window.manualValidateAndPreview() - Manual validation');
        console.log('   - window.checkButtonStates() - Check button states');
        console.log('   - window.manualCreateZone() - Manual zone creation');
        
        return {
            modal: !coordinateEntryManager.modal?.classList.contains('hidden'),
            geometryType: geometrySelect?.value,
            format: formatSelect?.value,
            coordinates: coordinatesInput?.value,
            zoneName: zoneNameInput?.value
        };
    }, 500);
};

// Quick test for validate button
window.testValidateButton = () => {
    const validateBtn = document.getElementById('validate-coordinates');
    if (validateBtn) {
        console.log('🧪 Testing validate button...');
        console.log('Button exists:', !!validateBtn);
        console.log('Button disabled:', validateBtn.disabled);
        console.log('Button onclick:', typeof validateBtn.onclick);
        console.log('Button style:', validateBtn.style.display);
        
        // Try to click it
        console.log('🖱️ Attempting to click validate button...');
        validateBtn.click();
        
        return true;
    } else {
        console.error('❌ Validate button not found');
        return false;
    }
};

export default coordinateEntryManager;