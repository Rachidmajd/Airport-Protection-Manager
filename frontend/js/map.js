// Map management and drawing functionality
import apiClient from './api.js';
// import { showDroneZoneForm, updateConflictPanel } from './ui.js';
import { showDroneZoneForm, updateConflictPanel, showNotification } from './ui.js';

class MapManager {
    constructor() {
        this.map = null;
        this.layers = {
            procedures: L.layerGroup(),
            droneZones: L.layerGroup(),
            conflicts: L.layerGroup()
        };
        this.drawControl = null;
        this.drawnItems = new L.FeatureGroup();
        this.currentDrawnLayer = null;
        this.procedures = [];
        this.droneZones = [];
        this.conflicts = [];
        this.currentDrawer = null;
    }

    init(mapElementId) {
        console.log('🗺️ Initializing map...');
        
        // Initialize map
        this.map = L.map(mapElementId, {
            center: [31.606701, -8.034632], // NYC area
            zoom: 10,
            zoomControl: true,
            preferCanvas: false
        });

        // Add base layer
        L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
            attribution: '© <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',
            maxZoom: 18,
            minZoom: 3
        }).addTo(this.map);

        // Add layer groups to map in correct order
        this.map.addLayer(this.layers.procedures);
        this.map.addLayer(this.layers.droneZones);
        this.map.addLayer(this.layers.conflicts);
        this.map.addLayer(this.drawnItems);

        // Setup debug logging for layers
        this.setupLayerDebuggging();

        // Setup mouse coordinate display
        this.setupCoordinateDisplay();

        // Setup drawing controls
        this.setupDrawingControls();

        // Load initial data
        this.loadData();

        console.log('✅ Map initialized successfully');
    }

    acceptExternalGeometry(feature, formData) {
        // This uses the existing createDroneZone method
        this.createDroneZone(feature, formData);
    }

    validatePolygonCoordinates(coordinates) {
        // Check if coordinates form a valid polygon
        if (coordinates.length < 3) {
            return { valid: false, error: 'At least 3 points required' };
        }
        
        // Check if all coordinates are valid
        for (const coord of coordinates) {
            if (!Array.isArray(coord) || coord.length < 2) {
                return { valid: false, error: 'Invalid coordinate format' };
            }
            
            const [lng, lat] = coord;
            if (lng < -180 || lng > 180 || lat < -90 || lat > 90) {
                return { valid: false, error: `Invalid coordinates: ${lng}, ${lat}` };
            }
        }
        
        return { valid: true };
    }

    setupLayerDebuggging() {
        this.map.on('layeradd', (e) => {
            console.log('➕ Layer added:', e.layer.constructor.name);
        });

        this.map.on('layerremove', (e) => {
            console.log('➖ Layer removed:', e.layer.constructor.name);
        });
    }

    setupCoordinateDisplay() {
        const coordsElement = document.getElementById('map-coords');
        
        if (coordsElement) {
            this.map.on('mousemove', (e) => {
                const lat = e.latlng.lat.toFixed(6);
                const lng = e.latlng.lng.toFixed(6);
                coordsElement.textContent = `Lat: ${lat}, Lng: ${lng}`;
            });
        }
    }

    setupDrawingControls() {
        console.log('🎨 Setting up drawing controls...');
    
        // Setup drawing event handlers
        this.map.on('draw:created', (e) => {
            console.log('✏️ Shape created:', e.layerType);
            this.handleDrawCreated(e);
        });
    
        this.map.on('draw:drawstart', (e) => {
            console.log('🎯 Drawing started:', e.layerType);
            this.updateDrawingButtonText(true, e.layerType);
        });
    
        this.map.on('draw:drawstop', (e) => {
            console.log('🛑 Drawing stopped:', e.layerType);
            this.updateDrawingButtonText(false);
            // Clean up the drawer reference
            if (this.currentDrawer) {
                this.currentDrawer = null;
            }
        });
    
        this.map.on('draw:drawvertex', (e) => {
            console.log('📍 Vertex added');
        });
    
        this.map.on('draw:canceled', (e) => {
            console.log('❌ Drawing canceled:', e.layerType);
            this.handleDrawCanceled();
        });
    
        // Setup drawing tool buttons
        this.setupDrawingButtons();
    }

    handleDrawCreated(e) {
        console.log('📝 Processing created shape:', e.layerType);
        
        // Store the current drawn layer
        this.currentDrawnLayer = e.layer;
        
        // IMPORTANT: Disable and clean up the drawer immediately
        if (this.currentDrawer) {
            try {
                this.currentDrawer.disable();
            } catch (error) {
                console.warn('Error disabling drawer:', error);
            }
            this.currentDrawer = null;
        }
        
        // Re-enable drawing buttons
        this.setDrawingButtonsState(false);
        
        // Add to drawnItems immediately for visibility
        this.drawnItems.addLayer(this.currentDrawnLayer);
        console.log('🐛 Added temporary layer to map');
        
        // Convert to GeoJSON
        const geometry = this.layerToGeoJSON(e.layer);
        
        console.log('🐛 Geometry conversion result:', geometry);
        
        if (geometry) {
            console.log('📐 Geometry created successfully:', geometry.geometry.type);
            
            showDroneZoneForm(geometry, (formData) => {
                console.log('📤 Form callback triggered with data:', formData);
                this.createDroneZone(geometry, formData);
            });
        } else {
            console.error('❌ Failed to create geometry from layer');
            this.cleanupFailedDraw();
            alert('Failed to create drone zone geometry. Please try again.');
        }
        
        // Reset drawing buttons
        this.updateDrawingButtonText(false);
    }

    handleDrawCanceled() {
        console.log('🧹 Cleaning up canceled draw');
        
        // Clean up drawer
        if (this.currentDrawer) {
            try {
                this.currentDrawer.disable();
            } catch (e) {
                console.warn('Error disabling drawer on cancel:', e);
            }
            this.currentDrawer = null;
        }
        
        // Clean up any temporary layer
        this.cleanupFailedDraw();
        
        // Reset buttons
        this.setDrawingButtonsState(false);
        this.updateDrawingButtonText(false);
    }

    cleanupFailedDraw() {
        if (this.currentDrawnLayer) {
            this.drawnItems.removeLayer(this.currentDrawnLayer);
            this.currentDrawnLayer = null;
        }
    }

    setupDrawingButtons() {
        console.log('🔘 Setting up drawing buttons');
        
        const polygonBtn = document.getElementById('draw-polygon-btn');
        const circleBtn = document.getElementById('draw-circle-btn');
        const pointBtn = document.getElementById('draw-point-btn'); // NEW

        if (polygonBtn) {
            polygonBtn.addEventListener('click', (e) => {
                e.preventDefault();
                console.log('🔘 Polygon button clicked');
                this.startDrawing('polygon');
            });
        } else {
            console.warn('❌ Polygon button not found');
        }

        if (circleBtn) {
            circleBtn.addEventListener('click', (e) => {
                e.preventDefault();
                console.log('🔘 Circle button clicked');
                this.startDrawing('circle');
            });
        } else {
            console.warn('❌ Circle button not found');
        }

        if (pointBtn) {
            pointBtn.addEventListener('click', (e) => {
                e.preventDefault();
                console.log('🔘 Point button clicked');
                this.startDrawing('point');
            });
        } else {
            console.warn('❌ Point button not found');
        }
    }

    startDrawing(type) {
        console.log(`🎨 Starting ${type} drawing`);
        
        // Cancel any existing drawing
        this.cancelCurrentDrawing();
        
        // Ensure clean state
        if (this.currentDrawer) {
            console.warn('⚠️ Drawer still exists after cancel, forcing cleanup');
            this.currentDrawer = null;
        }
        
        // Clean up any temporary layers
        if (this.currentDrawnLayer) {
            this.drawnItems.removeLayer(this.currentDrawnLayer);
            this.currentDrawnLayer = null;
        }
        
        // Disable buttons during drawing
        this.setDrawingButtonsState(true);
    
        try {
            if (type === 'polygon') {
                this.currentDrawer = new L.Draw.Polygon(this.map, {
                    allowIntersection: false,
                    showArea: true,
                    drawError: {
                        color: '#e74c3c',
                        message: '<strong>Error:</strong> shape edges cannot cross!'
                    },
                    shapeOptions: {
                        color: '#3b82f6',
                        weight: 3,
                        fillOpacity: 0.2,
                        fillColor: '#3b82f6'
                    }
                });
            } else if (type === 'circle') {
                this.currentDrawer = new L.Draw.Circle(this.map, {
                    showRadius: true,
                    metric: true,
                    feet: false,
                    shapeOptions: {
                        color: '#3b82f6',
                        weight: 3,
                        fillOpacity: 0.2,
                        fillColor: '#3b82f6'
                    }
                });
            } else if (type === 'point') {  // NEW: Point drawing
                this.currentDrawer = new L.Draw.Marker(this.map, {
                    icon: new L.Icon({
                        iconUrl: 'https://unpkg.com/leaflet@1.9.4/dist/images/marker-icon.png',
                        iconSize: [25, 41],
                        iconAnchor: [12, 41],
                        popupAnchor: [1, -34],
                        shadowUrl: 'https://unpkg.com/leaflet@1.9.4/dist/images/marker-shadow.png',
                        shadowSize: [41, 41]
                    })
                });
            }
    
            if (this.currentDrawer) {
                this.currentDrawer.enable();
                console.log(`✅ ${type} drawer enabled`);
            }
        } catch (error) {
            console.error(`❌ Failed to enable ${type} drawer:`, error);
            this.currentDrawer = null; // Ensure it's null on error
            this.setDrawingButtonsState(false);
            alert(`Failed to start ${type} drawing. Please try again.`);
        }
    }

    resetDrawingState() {
        console.log('🔄 Resetting drawing state');
        
        // Disable current drawer
        if (this.currentDrawer) {
            try {
                this.currentDrawer.disable();
            } catch (e) {
                console.warn('Error disabling drawer:', e);
            }
            this.currentDrawer = null;
        }
        
        // Remove temporary layer
        if (this.currentDrawnLayer) {
            this.drawnItems.removeLayer(this.currentDrawnLayer);
            this.currentDrawnLayer = null;
        }
        
        // Clear all drawn items if needed
        this.drawnItems.clearLayers();
        
        // Reset buttons
        this.setDrawingButtonsState(false);
        this.updateDrawingButtonText(false);
        
        console.log('✅ Drawing state reset complete');
    }

    cancelCurrentDrawing() {
        if (this.currentDrawer) {
            try {
                this.currentDrawer.disable();
                console.log('🛑 Canceled current drawing');
            } catch (error) {
                console.warn('⚠️ Error canceling drawer:', error);
            }
            this.currentDrawer = null;
        }
    }

    setDrawingButtonsState(disabled) {
        const buttons = ['draw-polygon-btn', 'draw-circle-btn'];
        buttons.forEach(id => {
            const btn = document.getElementById(id);
            if (btn) {
                btn.disabled = disabled;
            }
        });
    }

    updateDrawingButtonText(drawing = false, type = '') {
        const polygonBtn = document.getElementById('draw-polygon-btn');
        const circleBtn = document.getElementById('draw-circle-btn');

        if (polygonBtn) {
            if (drawing && type === 'polygon') {
                polygonBtn.innerHTML = '<span>⏹️ Cancel</span>';
            } else {
                polygonBtn.innerHTML = '<span>📐 Polygon</span>';
            }
        }

        if (circleBtn) {
            if (drawing && type === 'circle') {
                circleBtn.innerHTML = '<span>⏹️ Cancel</span>';
            } else {
                circleBtn.innerHTML = '<span>⭕ Circle</span>';
            }
        }
    }

    layerToGeoJSON(layer) {
        console.log('🔄 Converting layer to GeoJSON:', layer.constructor.name);
        
        try {
            if (layer instanceof L.Marker) {  // NEW: Handle markers/points
                console.log('📍 Processing point/marker');
                const latLng = layer.getLatLng();
                
                return {
                    type: 'Feature',
                    properties: {
                        shapeType: 'point',
                        isPoint: true
                    },
                    geometry: {
                        type: 'Point',
                        coordinates: [latLng.lng, latLng.lat]
                    }
                };
            }
            else if (layer instanceof L.Polygon) {
                console.log('📐 Processing polygon');
                const latLngs = layer.getLatLngs()[0];
                const coordinates = latLngs.map(ll => [ll.lng, ll.lat]);
                coordinates.push(coordinates[0]); // Close the polygon
    
                return {
                    type: 'Feature',
                    properties: {
                        shapeType: 'polygon'
                    },
                    geometry: {
                        type: 'Polygon',
                        coordinates: [coordinates]
                    }
                };
            } 
            else if (layer instanceof L.Circle) {
                console.log('⭕ Processing circle');
                const center = layer.getLatLng();
                const radius = layer.getRadius();
                
                // Convert circle to polygon approximation
                const points = [];
                const numPoints = 64;
                
                for (let i = 0; i < numPoints; i++) {
                    const angle = (i * 360) / numPoints;
                    const radians = (angle * Math.PI) / 180;
                    
                    const latOffset = (radius / 111320) * Math.cos(radians);
                    const lngOffset = (radius / (111320 * Math.cos(center.lat * Math.PI / 180))) * Math.sin(radians);
                    
                    const lat = center.lat + latOffset;
                    const lng = center.lng + lngOffset;
                    points.push([lng, lat]);
                }
                points.push(points[0]);
    
                return {
                    type: 'Feature',
                    properties: { 
                        shapeType: 'circle',
                        isCircle: true, 
                        center: [center.lng, center.lat], 
                        radius: radius 
                    },
                    geometry: {
                        type: 'Polygon',
                        coordinates: [points]
                    }
                };
            }
            else {
                console.warn('⚠️ Unknown layer type:', layer.constructor.name);
                if (typeof layer.toGeoJSON === 'function') {
                    console.log('🔄 Using layer.toGeoJSON()');
                    return layer.toGeoJSON();
                }
            }
        } catch (error) {
            console.error('❌ Error converting layer to GeoJSON:', error);
        }
        
        return null;
    }

    async loadData() {
        try {
            console.log('📊 Loading map data...');
            
            // Create sample data since backend isn't ready
            await this.loadProceduresFromDatabase();
            
            this.droneZones = [];
            this.conflicts = [];

            // Render all data
            this.renderProcedures();
            this.renderDroneZones();
            this.renderConflicts();

            console.log('✅ Sample data loaded successfully');
        } catch (error) {
            console.error('❌ Failed to load map data:', error);
        }
    }

    // Add this new function to the MapManager class
loadAndRenderProjectGeometries(geometryCollection) {
    console.log('🔄 Loading and rendering project geometries...');
    
    // 1. Clear any existing drone zones from the map and local array
    this.layers.droneZones.clearLayers();
    this.droneZones = [];

    if (!geometryCollection || !geometryCollection.features || geometryCollection.features.length === 0) {
        console.log('✅ No existing geometries to render for this project.');
        return;
    }

    // 2. Iterate through the features and add them to the map
    geometryCollection.features.forEach(feature => {
        // Create a 'zone' object that matches the structure of newly drawn zones
        const zone = {
            id: 'proj_geom_' + Date.now() * Math.random(), // Create a temporary unique ID
            operationId: feature.properties.name || 'Loaded Geometry',
            altitudeRange: {
                min: feature.properties.altitude_min || 0,
                max: feature.properties.altitude_max || 400
            },
            status: 'Planned',
            geometry: feature // The feature itself is the geometry
        };

        // Add to the local array for consistency
        this.droneZones.push(zone);

        // Create and add the layer to the map
        const layer = this.createDroneZoneLayer(zone);
        if (layer) {
            this.layers.droneZones.addLayer(layer);
        }
    });

    console.log(`✅ Rendered ${this.droneZones.length} geometries from the project.`);
    
    // Optional: Zoom the map to fit all loaded geometries
    if (this.layers.droneZones.getLayers().length > 0) {
        this.map.fitBounds(this.layers.droneZones.getBounds(), { padding: [50, 50] });
    }
}

    async loadProceduresFromDatabase() {
        try {
            console.log('✈️ Loading flight procedures from database...');
            
            // Use the API client to fetch procedures
            const procedures = await window.apiClient.getProcedures({
                is_active: true,
                limit: 100
            });
            
            // Check data source
            const dataSource = window.apiClient.getDataSource();
            console.log(`📊 Loaded ${procedures.length} procedures from ${dataSource}`);
            
            // Store procedures
            this.procedures = procedures;
            
            // Log procedure details
            procedures.forEach(procedure => {
                console.log(`✈️ Loaded procedure: ${procedure.procedure_code} (${procedure.type}) - ${procedure.segmentCount} segments, ${procedure.protectionCount} protections`);
            });
            
            // Update UI if procedures are loaded successfully
            if (window.renderProcedureControls) {
                window.renderProcedureControls(this.procedures);
            }
            
        } catch (error) {
            console.error('❌ Failed to load procedures from database:', error);
            
            // Fallback to mock data
            console.log('🔄 Using fallback procedures...');
            this.procedures = this.createSampleProcedures();
        }
    }

    createSampleProcedures() {
        return [
            {
                id: 'proc_001',
                name: 'CANDR6 Departure',
                type: 'SID',
                runway: '04L',
                altitudeConstraints: { min: 1000, max: 18000 },
                isVisible: true,
                geometry: {
                    type: 'Feature',
                    properties: {},
                    geometry: {
                        type: 'LineString',
                        coordinates: [[-73.778925,40.639751],[-73.750000,40.680000],[-73.700000,40.750000],[-73.650000,40.820000]]
                    }
                }
            },
            {
                id: 'proc_002',
                name: 'LENDY4 Arrival',
                type: 'STAR',
                runway: '04L',
                altitudeConstraints: { min: 3000, max: 10000 },
                isVisible: true,
                geometry: {
                    type: 'Feature',
                    properties: {},
                    geometry: {
                        type: 'LineString',
                        coordinates: [[-73.400000,41.200000],[-73.500000,41.100000],[-73.600000,41.000000],[-73.700000,40.900000],[-73.778925,40.639751]]
                    }
                }
            }
        ];
    }

    renderProcedures() {
        console.log('✈️ Rendering flight procedures on map:', this.procedures.length);
        this.layers.procedures.clearLayers();
    
        if (!this.procedures || this.procedures.length === 0) {
            console.warn('⚠️ No procedures to render');
            return;
        }
    
        this.procedures.forEach(procedure => {
            if (procedure.isVisible === false) {
                console.log(`👁️ Skipping hidden procedure: ${procedure.procedure_code}`);
                return;
            }
            
            try {
                console.log(`🎨 Rendering procedure: ${procedure.procedure_code}`);
                
                // Render main procedure trajectory
                this.renderProcedureTrajectory(procedure);
                
                // Render individual segments if available
                // if (procedure.segments && procedure.segments.length > 0) {
                //     this.renderProcedureSegments(procedure);
                // }
                
                // Render protection areas if available
                if (procedure.protections) {
                    this.renderProcedureProtections(procedure);
                }
                
            } catch (error) {
                console.error(`❌ Error rendering procedure ${procedure.procedure_code}:`, error);
            }
        });
        
        console.log('✅ Procedures rendered on map');
    }

    // renderProcedureTrajectory(procedure) {
    //     if (!procedure.geometry) {
    //         console.warn(`⚠️ No geometry for procedure ${procedure.procedure_code}`);
    //         return;
    //     }
        
    //     try {
    //         const style = this.getProcedureStyle(procedure);
            
    //         const layer = L.geoJSON(procedure.geometry, {
    //             style: style,
    //             onEachFeature: (feature, layer) => {
    //                 // Add click handler
    //                 layer.on('click', (e) => {
    //                     console.log(`🖱️ Clicked procedure: ${procedure.procedure_code}`);
    //                     this.selectProcedure(procedure);
    //                     e.stopPropagation();
    //                 });
    
    //                 // Add hover effects
    //                 layer.on('mouseover', (e) => {
    //                     const hoverStyle = {
    //                         weight: 5,
    //                         opacity: 1,
    //                         color: style.color
    //                     };
    //                     e.target.setStyle(hoverStyle);
                        
    //                     // Show tooltip with procedure info
    //                     const tooltipContent = this.createProcedureTooltip(procedure);
    //                     layer.bindTooltip(tooltipContent, {
    //                         permanent: false,
    //                         direction: 'top',
    //                         className: 'procedure-tooltip'
    //                     }).openTooltip();
    //                 });
    
    //                 layer.on('mouseout', (e) => {
    //                     e.target.setStyle(style);
    //                     layer.closeTooltip();
    //                 });
    
    //                 // Add detailed popup
    //                 const popupContent = this.createProcedurePopup(procedure);
    //                 layer.bindPopup(popupContent, {
    //                     maxWidth: 400,
    //                     className: 'procedure-popup'
    //                 });
                    
    //                 // Store procedure reference in layer
    //                 layer._procedure = procedure;
    //             }
    //         });
    
    //         this.layers.procedures.addLayer(layer);
    //         console.log(`✅ Rendered trajectory for ${procedure.procedure_code}`);
            
    //     } catch (error) {
    //         console.error(`❌ Error rendering trajectory for ${procedure.procedure_code}:`, error);
    //     }
    // }
    
    renderProcedureTrajectory(procedure) {
        if (!procedure.geometry) return;
        
        let geometryToRender = procedure.geometry;
        
        // Handle FeatureCollection case
        if (geometryToRender.type === 'FeatureCollection') {
            // Convert to MultiLineString or handle multiple features
            const coordinates = geometryToRender.features
                .filter(f => f.geometry.type === 'LineString')
                .map(f => f.geometry.coordinates);
                
            geometryToRender = {
                type: 'MultiLineString',
                coordinates: coordinates
            };
        }
        
        const layer = L.geoJSON(geometryToRender, {
            style: this.getProcedureStyle(procedure)
        });
        
        this.layers.procedures.addLayer(layer);
    }


    // renderProcedureSegments(procedure) {
    //     console.log(`📍 Rendering ${procedure.segments.length} segments for ${procedure.procedure_code}`);
        
    //     procedure.segments.forEach((segment, index) => {
    //         try {
    //             if (!segment.trajectory_geometry) {
    //                 console.warn(`⚠️ No geometry for segment ${segment.segment_order}`);
    //                 return;
    //             }
                
    //             let segmentGeometry;
    //             if (typeof segment.trajectory_geometry === 'string') {
    //                 segmentGeometry = JSON.parse(segment.trajectory_geometry);
    //             } else {
    //                 segmentGeometry = segment.trajectory_geometry;
    //             }
                
    //             // Create GeoJSON feature for segment
    //             const segmentFeature = {
    //                 type: 'Feature',
    //                 properties: {
    //                     procedure_code: procedure.procedure_code,
    //                     segment_order: segment.segment_order,
    //                     segment_name: segment.segment_name,
    //                     waypoint_from: segment.waypoint_from,
    //                     waypoint_to: segment.waypoint_to,
    //                     altitude_min: segment.altitude_min,
    //                     altitude_max: segment.altitude_max
    //                 },
    //                 geometry: segmentGeometry
    //             };
                
    //             const style = {
    //                 ...this.getProcedureStyle(procedure),
    //                 weight: 2,
    //                 opacity: 0.6,
    //                 dashArray: '5, 5'
    //             };
                
    //             const segmentLayer = L.geoJSON(segmentFeature, {
    //                 style: style,
    //                 onEachFeature: (feature, layer) => {
    //                     // Segment-specific popup
    //                     const popupContent = this.createSegmentPopup(segment, procedure);
    //                     layer.bindPopup(popupContent, {
    //                         maxWidth: 300,
    //                         className: 'segment-popup'
    //                     });
                        
    //                     layer.on('mouseover', (e) => {
    //                         e.target.setStyle({ weight: 4, opacity: 1 });
    //                     });
                        
    //                     layer.on('mouseout', (e) => {
    //                         e.target.setStyle(style);
    //                     });
    //                 }
    //             });
                
    //             this.layers.procedures.addLayer(segmentLayer);
                
    //         } catch (error) {
    //             console.error(`❌ Error rendering segment ${segment.segment_order}:`, error);
    //         }
    //     });
    // }
    
    // renderProcedureProtections(procedure) {
    //     console.log(`🛡️ Rendering ${procedure.protections.length} protection areas for ${procedure.procedure_code}`);
        
    //     procedure.protections.forEach(protection => {
    //         try {
    //             if (!protection.protection_geometry) {
    //                 console.warn(`⚠️ No geometry for protection ${protection.protection_name}`);
    //                 return;
    //             }
                
    //             let protectionGeometry;
    //             if (typeof protection.protection_geometry === 'string') {
    //                 protectionGeometry = JSON.parse(protection.protection_geometry);
    //             } else {
    //                 protectionGeometry = protection.protection_geometry;
    //             }
                
    //             // Create GeoJSON feature for protection
    //             const protectionFeature = {
    //                 type: 'Feature',
    //                 properties: {
    //                     procedure_code: procedure.procedure_code,
    //                     protection_name: protection.protection_name,
    //                     protection_type: protection.protection_type,
    //                     restriction_level: protection.restriction_level,
    //                     conflict_severity: protection.conflict_severity,
    //                     analysis_priority: protection.analysis_priority
    //                 },
    //                 geometry: protectionGeometry
    //             };
                
    //             const style = this.getProtectionStyle(protection);
                
    //             const protectionLayer = L.geoJSON(protectionFeature, {
    //                 style: style,
    //                 onEachFeature: (feature, layer) => {
    //                     // Protection-specific popup
    //                     const popupContent = this.createProtectionPopup(protection, procedure);
    //                     layer.bindPopup(popupContent, {
    //                         maxWidth: 350,
    //                         className: 'protection-popup'
    //                     });
                        
    //                     layer.on('mouseover', (e) => {
    //                         e.target.setStyle({ 
    //                             weight: style.weight + 1, 
    //                             fillOpacity: style.fillOpacity + 0.2 
    //                         });
    //                     });
                        
    //                     layer.on('mouseout', (e) => {
    //                         e.target.setStyle(style);
    //                     });
    //                 }
    //             });
                
    //             this.layers.procedures.addLayer(protectionLayer);
                
    //         } catch (error) {
    //             console.error(`❌ Error rendering protection ${protection.protection_name}:`, error);
    //         }
    //     });
    // }

    // CRITICAL: Fixed createDroneZone method for persistent geometry
    
    
    // renderProcedureProtections(procedure) {
    //     if (!procedure.protections) return;
        
    //     // Handle both FeatureCollection and direct geometry
    //     let protectionData = procedure.protections;
        
    //     if (protectionData.type !== 'FeatureCollection') {
    //         // Wrap single geometry in FeatureCollection
    //         protectionData = {
    //             type: 'FeatureCollection',
    //             features: [{ type: 'Feature', properties: {}, geometry: protectionData }]
    //         };
    //     }
        
    //     const layer = L.geoJSON(protectionData, {
    //         style: feature => this.getProtectionStyle(feature.properties),
    //         onEachFeature: (feature, layer) => {
    //             // Add protection-specific popup
    //             const popupContent = this.createProtectionPopup(feature.properties,procedure);
    //             layer.bindPopup(popupContent);
    //         }
    //     });
        
    //     this.layers.procedures.addLayer(layer);
    // }

    renderProcedureProtections(procedure) {
        if (!procedure.protections || !procedure.protections.features || procedure.protections.features.length === 0) {
            return;
        }
    
        let geometryToRender = procedure.protections;
    
        // ** NEW ROBUST LOGIC **
        // Check if it's a FeatureCollection and rebuild it into a single MultiPolygon
        if (geometryToRender.type === 'FeatureCollection') {
            const coordinates = geometryToRender.features
                // Filter for features that are Polygons or MultiPolygons
                .filter(f => f.geometry && (f.geometry.type === 'Polygon' || f.geometry.type === 'MultiPolygon'))
                // Extract the coordinate arrays
                .flatMap(f => f.geometry.type === 'Polygon' ? [f.geometry.coordinates] : f.geometry.coordinates);
    
            geometryToRender = {
                type: 'Feature',
                properties: {}, // Style properties will be applied per-feature later
                geometry: {
                    type: 'MultiPolygon',
                    coordinates: coordinates
                }
            };
        }
    
        const layer = L.geoJSON(geometryToRender, {
            style: feature => this.getProtectionStyle(procedure), // Use procedure properties for styling
            onEachFeature: (feature, layer) => {
                // The popup now gets properties from the parent procedure
                const popupContent = this.createProtectionPopup(procedure, procedure);
                layer.bindPopup(popupContent);
            }
        });
    
        this.layers.procedures.addLayer(layer);
    }
    
    
    
    async createDroneZone(geometry, formData) {
        try {
            console.log('🚁 Creating drone zone with data:', formData);
            console.log('📐 Using geometry:', geometry.geometry.type);
            
            // Handle point geometries by converting to circular polygon
            let finalGeometry = geometry;
            if (geometry.geometry.type === 'Point') {
                const radius = formData.pointRadius || 100; // Default 100m radius
                finalGeometry = this.convertPointToPolygon(geometry, radius);
                console.log('📍 Converted point to polygon with radius:', radius);
            }
            
            // Create the new drone zone object
            const newZone = {
                id: 'zone_' + Date.now(),
                operationId: formData.operationId,
                altitudeRange: {
                    min: parseInt(formData.altitudeMin),
                    max: parseInt(formData.altitudeMax)
                },
                startTime: formData.startTime,
                endTime: formData.endTime,
                status: formData.status,
                geometry: finalGeometry,
                originalGeometry: geometry, // Store original for reference
                createdAt: new Date().toISOString()
            };
    
            // Add to drone zones array
            this.droneZones.push(newZone);
            console.log('📝 Added to droneZones array. Total zones:', this.droneZones.length);
            
            // Create and add the persistent layer
            const persistentLayer = this.createDroneZoneLayer(newZone);
            if (persistentLayer) {
                this.layers.droneZones.addLayer(persistentLayer);
                console.log('✅ Added persistent layer to drone zones layer group');
            }
            
            // Remove the temporary drawn layer
            if (this.currentDrawnLayer) {
                console.log('🧹 Removing temporary drawn layer');
                this.drawnItems.removeLayer(this.currentDrawnLayer);
                this.currentDrawnLayer = null;
            }
            
            // Focus on the new zone
            this.focusOnDroneZone(newZone);
            
            console.log('✅ Drone zone created successfully:', newZone.operationId);
            
            if (window.uiManager && window.uiManager.renderProjectTree) {
                window.uiManager.renderProjectTree();
            }

            return newZone;
            
        } catch (error) {
            console.error('❌ Failed to create drone zone:', error);
            
            if (this.currentDrawnLayer) {
                this.drawnItems.removeLayer(this.currentDrawnLayer);
                this.currentDrawnLayer = null;
            }
            
            throw error;
        }
    }

    convertPointToPolygon(pointFeature, radius) {
        const coords = pointFeature.geometry.coordinates;
        const center = { lat: coords[1], lng: coords[0] };
        
        const points = [];
        const numPoints = 32; // Less points needed for small circles
        
        for (let i = 0; i < numPoints; i++) {
            const angle = (i * 360) / numPoints;
            const radians = (angle * Math.PI) / 180;
            
            const latOffset = (radius / 111320) * Math.cos(radians);
            const lngOffset = (radius / (111320 * Math.cos(center.lat * Math.PI / 180))) * Math.sin(radians);
            
            const lat = center.lat + latOffset;
            const lng = center.lng + lngOffset;
            points.push([lng, lat]);
        }
        points.push(points[0]); // Close the polygon
        
        return {
            type: 'Feature',
            properties: {
                ...pointFeature.properties,
                originalType: 'Point',
                radius: radius,
                center: coords
            },
            geometry: {
                type: 'Polygon',
                coordinates: [points]
            }
        };
    }

    createDroneZoneLayer(zone) {
        try {
            console.log('🎨 Creating persistent layer for zone:', zone.operationId);
            
            const style = this.getDroneZoneStyle(zone);
            
            // Check if this was originally a point
            if (zone.originalGeometry && zone.originalGeometry.geometry.type === 'Point') {
                // Add a marker at the center point as well
                const coords = zone.originalGeometry.geometry.coordinates;
                const marker = L.marker([coords[1], coords[0]], {
                    icon: L.divIcon({
                        className: 'drone-point-marker',
                        html: `<div style="
                            background: ${style.color};
                            width: 12px;
                            height: 12px;
                            border-radius: 50%;
                            border: 2px solid white;
                            box-shadow: 0 2px 4px rgba(0,0,0,0.3);
                        "></div>`,
                        iconSize: [16, 16],
                        iconAnchor: [8, 8]
                    })
                });
                
                // Add marker to the layer group
                this.layers.droneZones.addLayer(marker);
            }
            
            // Continue with normal polygon creation
            const layer = L.geoJSON(zone.geometry, {
                style: style,
                onEachFeature: (feature, layerFeature) => {
                    // ... existing event handlers ...
                }
            });
            
            console.log('✅ Persistent layer created for:', zone.operationId);
            return layer;
            
        } catch (error) {
            console.error('❌ Failed to create persistent layer for zone:', zone.operationId, error);
            return null;
        }
    }

    renderDroneZones() {
        console.log('🚁 Rendering all drone zones:', this.droneZones.length);
        
        // Clear existing layers
        this.layers.droneZones.clearLayers();

        // Add each zone as a layer
        this.droneZones.forEach((zone, index) => {
            console.log(`🎨 Rendering zone ${index + 1}/${this.droneZones.length}:`, zone.operationId);
            
            const layer = this.createDroneZoneLayer(zone);
            if (layer) {
                this.layers.droneZones.addLayer(layer);
                console.log('✅ Zone rendered:', zone.operationId);
            } else {
                console.error('❌ Failed to render zone:', zone.operationId);
            }
        });
        
        const finalLayerCount = this.layers.droneZones.getLayers().length;
        console.log('✅ All drone zones rendered. Final layer count:', finalLayerCount);
        
        // Verify layers are on the map
        if (this.map.hasLayer(this.layers.droneZones)) {
            console.log('✅ Drone zones layer group is on the map');
        } else {
            console.warn('⚠️ Drone zones layer group is NOT on the map');
        }
    }

    renderConflicts() {
        console.log('⚠️ Rendering conflicts:', this.conflicts.length);
        this.layers.conflicts.clearLayers();

        this.conflicts.forEach(conflict => {
            if (conflict.geometry) {
                const layer = L.geoJSON(conflict.geometry, {
                    style: {
                        color: '#dc2626',
                        weight: 3,
                        opacity: 0.8,
                        fillOpacity: 0.4,
                        fillColor: '#dc2626'
                    },
                    onEachFeature: (feature, layer) => {
                        const popupContent = this.createConflictPopup(conflict);
                        layer.bindPopup(popupContent);
                    }
                });

                this.layers.conflicts.addLayer(layer);
            }
        });

        // Update conflict panel
        updateConflictPanel(this.conflicts, (conflict) => {
            this.focusOnConflict(conflict);
        });
        
        console.log('✅ Conflicts rendered');
    }

    getProcedureStyle(procedure) {
        const baseStyle = {
            weight: 3,
            opacity: 0.8,
            fillOpacity: 0
        };
    
        // Color by procedure type
        switch (procedure.type) {
            case 'SID':
            case 'DEPARTURE':
                return { ...baseStyle, color: '#10b981' }; // Green
            case 'STAR':
            case 'ARRIVAL':
                return { ...baseStyle, color: '#f59e0b' }; // Amber
            case 'APPROACH':
                return { ...baseStyle, color: '#8b5cf6' }; // Purple
            default:
                return { ...baseStyle, color: '#6b7280' }; // Gray
        }
    }

    getProtectionStyle(protection) {
        let color = '#3b82f6';
        let fillOpacity = 0.2;
        
        // Color by restriction level
        switch (protection.restriction_level) {
            case 'prohibited':
                color = '#dc2626';
                fillOpacity = 0.3;
                break;
            case 'restricted':
                color = '#f59e0b';
                fillOpacity = 0.25;
                break;
            case 'caution':
                color = '#eab308';
                fillOpacity = 0.2;
                break;
            case 'advisory':
                color = '#3b82f6';
                fillOpacity = 0.15;
                break;
            case 'monitoring':
                color = '#6b7280';
                fillOpacity = 0.1;
                break;
            default:
                color = '#3b82f6';
        }
    
        return {
            color: color,
            weight: 2,
            opacity: 0.7,
            fillOpacity: fillOpacity,
            fillColor: color,
            fillRule: 'nonzero'
        };
    }
    
    createProcedureTooltip(procedure) {
        return `
            <div style="font-weight: 600; margin-bottom: 4px;">${procedure.procedure_code}</div>
            <div style="font-size: 12px; color: #6b7280;">
                ${procedure.type} | ${procedure.airport_icao}${procedure.runway ? ` RWY ${procedure.runway}` : ''}
            </div>
        `;
    }
    
    createProcedurePopup(procedure) {
        let segmentsInfo = '';
        if (procedure.segments && procedure.segments.length > 0) {
            segmentsInfo = `
                <div style="margin-top: 10px;">
                    <strong>Segments (${procedure.segments.length}):</strong>
                    <div style="max-height: 100px; overflow-y: auto; margin-top: 5px;">
                        ${procedure.segments.map(segment => `
                            <div style="font-size: 11px; padding: 2px 0; border-bottom: 1px solid #e5e7eb;">
                                ${segment.segment_order}. ${segment.segment_name || 'Unnamed'} 
                                (${segment.waypoint_from || '?'} → ${segment.waypoint_to || '?'})
                                ${segment.altitude_min || segment.altitude_max ? 
                                    `<br><span style="color: #6b7280;">Alt: ${segment.altitude_min || 0}-${segment.altitude_max || '∞'} ft</span>` : ''}
                            </div>
                        `).join('')}
                    </div>
                </div>
            `;
        }
        
        let protectionsInfo = '';
        if (procedure.protections && procedure.protections.features && procedure.protections.features.length > 0) {
            protectionsInfo = `
                <div style="margin-top: 10px;">
                    {/* Get the count from the .features array's length */}
                    <strong>Protection Areas (${procedure.protections.features.length}):</strong>
                    <div style="max-height: 80px; overflow-y: auto; margin-top: 5px;">
                        {/* Use .map() on the .features array */}
                        ${procedure.protections.features.map(feature => {
                            const protection = feature.properties; // The properties are inside each feature
                            return `
                                <div style="font-size: 11px; padding: 2px 0; border-bottom: 1px solid #e5e7eb;">
                                    ${protection.protection_name} 
                                    <span style="background: #fef3c7; color: #92400e; padding: 1px 4px; border-radius: 4px; font-size: 10px;">
                                        ${protection.restriction_level}
                                    </span>
                                </div>
                            `;
                        }).join('')}
                    </div>
                </div>
            `;
        }
        
        return `
            <div class="popup-content" style="min-width: 250px;">
                <div style="font-weight: 600; font-size: 14px; color: #1e40af; margin-bottom: 8px;">
                    ${procedure.procedure_code}
                </div>
                <div style="margin-bottom: 8px;">
                    <strong>${procedure.name}</strong>
                </div>
                <div style="font-size: 12px; color: #6b7280; margin-bottom: 8px;">
                    <strong>Type:</strong> ${procedure.type}<br>
                    <strong>Airport:</strong> ${procedure.airport_icao}${procedure.runway ? ` | <strong>Runway:</strong> ${procedure.runway}` : ''}<br>
                    ${procedure.altitudeConstraints && (procedure.altitudeConstraints.min || procedure.altitudeConstraints.max) ? 
                        `<strong>Altitude:</strong> ${procedure.altitudeConstraints.min || 0}-${procedure.altitudeConstraints.max || '∞'} ft` : ''}
                </div>
                ${segmentsInfo}
                ${protectionsInfo}
                <div style="margin-top: 10px; padding-top: 8px; border-top: 1px solid #e5e7eb;">
                    <button onclick="mapManager.focusOnProcedure('${procedure.id}')" style="
                        background: #1e40af; color: white; border: none; padding: 4px 8px; 
                        border-radius: 4px; font-size: 11px; cursor: pointer; margin-right: 5px;
                    ">Focus</button>
                    <button onclick="mapManager.toggleProcedureVisibility('${procedure.id}', false)" style="
                        background: #6b7280; color: white; border: none; padding: 4px 8px; 
                        border-radius: 4px; font-size: 11px; cursor: pointer;
                    ">Hide</button>
                </div>
            </div>
        `;
    }
    
    createSegmentPopup(segment, procedure) {
        return `
            <div class="popup-content">
                <div style="font-weight: 600; color: #1e40af; margin-bottom: 5px;">
                    Segment ${segment.segment_order}
                </div>
                <div style="margin-bottom: 8px;">
                    <strong>${segment.segment_name || 'Unnamed Segment'}</strong>
                </div>
                <div style="font-size: 12px; color: #6b7280;">
                    <strong>Procedure:</strong> ${procedure.procedure_code}<br>
                    <strong>Route:</strong> ${segment.waypoint_from || '?'} → ${segment.waypoint_to || '?'}<br>
                    ${segment.altitude_min || segment.altitude_max ? 
                        `<strong>Altitude:</strong> ${segment.altitude_min || 0}-${segment.altitude_max || '∞'} ft<br>` : ''}
                    ${segment.altitude_restriction ? 
                        `<strong>Restriction:</strong> ${segment.altitude_restriction}<br>` : ''}
                    ${segment.speed_limit ? 
                        `<strong>Speed:</strong> ${segment.speed_limit} kts<br>` : ''}
                    ${segment.magnetic_course ? 
                        `<strong>Course:</strong> ${segment.magnetic_course}°<br>` : ''}
                    ${segment.turn_direction && segment.turn_direction !== 'straight' ? 
                        `<strong>Turn:</strong> ${segment.turn_direction}<br>` : ''}
                </div>
            </div>
        `;
    }
    
    createProtectionPopup(protection, procedure) {
        return `
            <div class="popup-content">
                <div style="font-weight: 600; color: #1e40af; margin-bottom: 5px;">
                    ${protection.protection_name}
                </div>
                <div style="font-size: 12px; color: #6b7280; margin-bottom: 8px;">
                    <strong>Procedure:</strong> ${procedure.procedure_code}
                </div>
                <div style="font-size: 12px;">
                    <strong>Type:</strong> ${protection.protection_type}<br>
                    <strong>Restriction:</strong> 
                    <span style="background: #fef3c7; color: #92400e; padding: 2px 6px; border-radius: 4px; font-size: 11px;">
                        ${protection.restriction_level}
                    </span><br>
                    <strong>Severity:</strong> 
                    <span style="background: #fee2e2; color: #991b1b; padding: 2px 6px; border-radius: 4px; font-size: 11px;">
                        ${protection.conflict_severity}
                    </span><br>
                    <strong>Priority:</strong> ${protection.analysis_priority}<br>
                    ${protection.altitude_min || protection.altitude_max ? 
                        `<strong>Altitude:</strong> ${protection.altitude_min || 0}-${protection.altitude_max || '∞'} ft ${protection.altitude_reference || 'MSL'}<br>` : ''}
                    ${protection.regulatory_source ? 
                        `<strong>Authority:</strong> ${protection.regulatory_source}<br>` : ''}
                </div>
                ${protection.operational_notes ? `
                    <div style="margin-top: 8px; padding: 6px; background: #f8fafc; border-radius: 4px; font-size: 11px;">
                        <strong>Notes:</strong> ${protection.operational_notes}
                    </div>
                ` : ''}
            </div>
        `;
    }

    getDroneZoneStyle(zone) {
        let color = '#3b82f6';
        let fillOpacity = 0.3;
        
        switch (zone.status) {
            case 'Active':
                color = '#10b981';
                fillOpacity = 0.4;
                break;
            case 'Planned':
                color = '#f59e0b';
                fillOpacity = 0.3;
                break;
            case 'Completed':
                color = '#6b7280';
                fillOpacity = 0.2;
                break;
            default:
                color = '#3b82f6';
        }

        return {
            color: color,
            weight: 2,
            opacity: 0.8,
            fillOpacity: fillOpacity,
            fillColor: color
        };
    }


    createDroneZonePopup(zone) {
        return `
            <div class="popup-content">
                <div class="popup-title">Drone Zone: ${zone.operationId}</div>
                <div class="popup-info"><strong>Status:</strong> ${zone.status}</div>
                <div class="popup-info"><strong>Altitude:</strong> ${zone.altitudeRange.min}-${zone.altitudeRange.max} ft AGL</div>
                <div class="popup-info"><strong>Start:</strong> ${new Date(zone.startTime).toLocaleString()}</div>
                <div class="popup-info"><strong>End:</strong> ${new Date(zone.endTime).toLocaleString()}</div>
                <div class="popup-actions">
                    <button class="popup-button edit" onclick="mapManager.editDroneZone('${zone.id}')">Edit</button>
                    <button class="popup-button delete" onclick="mapManager.deleteDroneZone('${zone.id}')">Delete</button>
                </div>
            </div>
        `;
    }

    createConflictPopup(conflict) {
        return `
            <div class="popup-content">
                <div class="popup-title">⚠️ Conflict Detected</div>
                <div class="popup-info"><strong>Type:</strong> ${conflict.conflictType}</div>
                <div class="popup-info"><strong>Severity:</strong> ${conflict.severity}</div>
                <div class="popup-info">${conflict.description}</div>
                <div class="popup-info"><strong>Detected:</strong> ${new Date(conflict.detectedAt).toLocaleString()}</div>
            </div>
        `;
    }

    async deleteDroneZone(zoneId) {
        if (!confirm('Are you sure you want to delete this drone zone?')) {
            return;
        }

        try {
            console.log('🗑️ Deleting drone zone:', zoneId);
            
            const initialCount = this.droneZones.length;
            this.droneZones = this.droneZones.filter(z => z.id !== zoneId);
            
            if (this.droneZones.length < initialCount) {
                console.log('✅ Removed from droneZones array');
                this.renderDroneZones();
                if (window.uiManager && window.uiManager.renderProjectTree) {
                    console.log('🌳 Updating project tree after zone deletion...');
                    window.uiManager.renderProjectTree();
                }
                console.log('✅ Drone zone deleted successfully');
            } else {
                console.warn('⚠️ Zone not found in array:', zoneId);
            }
            
        } catch (error) {
            console.error('❌ Failed to delete drone zone:', error);
            alert('Failed to delete drone zone: ' + error.message);
        }
    }

    async editDroneZone(zoneId) {
        console.log(`✏️ Editing drone zone: ${zoneId}`);
    
        // 1. Find the drone zone object from the main array
        const zoneToEdit = this.droneZones.find(z => z.id === zoneId);
        if (!zoneToEdit) {
            console.error(`❌ Drone zone with ID ${zoneId} not found for editing.`);
            showNotification('Could not find the drone zone to edit.', 'error');
            return;
        }
    
        // 2. Define the callback function that will run when the form is submitted
        const updateCallback = (updatedData) => {
            console.log(`💾 Saving changes for zone: ${zoneId}`, updatedData);
    
            // Find the index of the zone to update it in the array
            const zoneIndex = this.droneZones.findIndex(z => z.id === zoneId);
            if (zoneIndex === -1) {
                console.error(`❌ Drone zone with ID ${zoneId} disappeared during edit.`);
                showNotification('An error occurred while saving the zone.', 'error');
                return;
            }
    
            // Merge the updated data into the existing zone object in the array
            this.droneZones[zoneIndex] = {
                ...this.droneZones[zoneIndex], // Keep original id, geometry, etc.
                operationId: updatedData.operationId,
                altitudeRange: {
                    min: parseInt(updatedData.altitudeMin),
                    max: parseInt(updatedData.altitudeMax)
                },
                startTime: updatedData.startTime,
                endTime: updatedData.endTime,
                status: updatedData.status,
            };
    
            // 3. Re-render all drone zones to reflect the change
            this.renderDroneZones();
            
            console.log(`✅ Zone ${zoneId} updated successfully.`);
            showNotification(`Drone Zone "${updatedData.operationId}" updated.`, 'success');
        };
    
        // 4. Call the UI function to show the form, passing the existing data to pre-populate it
        showDroneZoneForm(zoneToEdit.geometry, updateCallback, zoneToEdit);
    }

    async refreshConflicts() {
        try {
            console.log('🔄 Refreshing conflicts...');
            // This will work when backend is ready
        } catch (error) {
            console.error('❌ Failed to refresh conflicts:', error);
        }
    }

    selectProcedure(procedure) {
        console.log('✈️ Selected procedure:', procedure.procedure_code);
        
        // Store selected procedure
        this.selectedProcedure = procedure;
        
        // Highlight the procedure on map
        this.highlightProcedure(procedure);
        
        // Update UI if available
        if (window.showNotification) {
            window.showNotification(`Selected procedure: ${procedure.procedure_code} - ${procedure.name}`, 'info');
        }
        
        // Trigger custom event for other components
        document.dispatchEvent(new CustomEvent('procedureSelected', {
            detail: { procedure: procedure }
        }));
    }

    highlightProcedure(procedure) {
        // Remove previous highlights
        this.clearProcedureHighlights();
        
        // Find and highlight the procedure layers
        this.layers.procedures.eachLayer(layer => {
            if (layer._procedure && layer._procedure.id === procedure.id) {
                layer.setStyle({
                    weight: 5,
                    opacity: 1,
                    color: '#dc2626' // Red highlight
                });
                layer._isHighlighted = true;
            }
        });
    }

    clearProcedureHighlights() {
        this.layers.procedures.eachLayer(layer => {
            if (layer._isHighlighted) {
                const originalStyle = this.getProcedureStyle(layer._procedure);
                layer.setStyle(originalStyle);
                layer._isHighlighted = false;
            }
        });
    }

    focusOnProcedure(procedureId) {
        console.log(`🎯 Focusing on procedure: ${procedureId}`);
        
        const procedure = this.procedures.find(p => p.id == procedureId);
        if (!procedure || !procedure.geometry) {
            console.warn(`⚠️ Cannot focus on procedure ${procedureId} - no geometry`);
            return;
        }
        
        try {
            const layer = L.geoJSON(procedure.geometry);
            const bounds = layer.getBounds();
            
            if (bounds.isValid()) {
                this.map.fitBounds(bounds, { 
                    padding: [20, 20],
                    maxZoom: 13
                });
                
                // Highlight the procedure
                this.selectProcedure(procedure);
            } else {
                console.warn(`⚠️ Invalid bounds for procedure ${procedureId}`);
            }
        } catch (error) {
            console.error(`❌ Error focusing on procedure ${procedureId}:`, error);
        }
    }

    toggleProcedureVisibility(procedureId, visible) {
        console.log(`👁️ Toggling procedure ${procedureId} visibility: ${visible}`);
        
        const procedure = this.procedures.find(p => p.id == procedureId);
        if (procedure) {
            procedure.isVisible = visible;
            this.renderProcedures();
            
            // Update procedure controls if available
            if (window.renderProcedureControls) {
                window.renderProcedureControls(this.procedures);
            }
            
            console.log(`👁️ Procedure ${procedureId} visibility set to ${visible}`);
        }
    }

    selectDroneZone(zone) {
        console.log('🚁 Selected drone zone:', zone.operationId);
    }

    focusOnConflict(conflict) {
        if (conflict.geometry) {
            const layer = L.geoJSON(conflict.geometry);
            const bounds = layer.getBounds();
            this.map.fitBounds(bounds, { padding: [20, 20] });
        }
    }

    focusOnDroneZone(zone) {
        if (zone.geometry) {
            console.log('🎯 Focusing on drone zone:', zone.operationId);
            const layer = L.geoJSON(zone.geometry);
            const bounds = layer.getBounds();
            this.map.fitBounds(bounds, { 
                padding: [20, 20],
                maxZoom: 15
            });
        }
    }

    // Method to refresh procedures from database
    async refreshProcedures() {
        try {
            console.log('🔄 Refreshing procedures from database...');
            
            await this.loadProceduresFromDatabase();
            this.renderProcedures();
            
            if (window.showNotification) {
                window.showNotification(`Refreshed ${this.procedures.length} procedures from database`, 'success');
            }
            
            console.log('✅ Procedures refreshed');
        } catch (error) {
            console.error('❌ Failed to refresh procedures:', error);
            
            if (window.showNotification) {
                window.showNotification('Failed to refresh procedures', 'error');
            }
        }
    }

    // Method to filter procedures by type
    filterProceduresByType(type) {
        console.log(`🔍 Filtering procedures by type: ${type}`);
        
        this.procedures.forEach(procedure => {
            if (type === 'all' || procedure.type === type) {
                procedure.isVisible = true;
            } else {
                procedure.isVisible = false;
            }
        });
        
        this.renderProcedures();
        
        // Update procedure controls
        if (window.renderProcedureControls) {
            window.renderProcedureControls(this.procedures);
        }
    }

    // Method to get procedure statistics
    getProcedureStats() {
        const stats = {
            total: this.procedures.length,
            visible: this.procedures.filter(p => p.isVisible).length,
            byType: {},
            totalSegments: 0,
            totalProtections: 0,
            fromDatabase: window.apiClient.isUsingDatabase()
        };
        
        this.procedures.forEach(procedure => {
            // Count by type
            if (!stats.byType[procedure.type]) {
                stats.byType[procedure.type] = 0;
            }
            stats.byType[procedure.type]++;
            
            // Count segments and protections
            stats.totalSegments += procedure.segmentCount || 0;
            stats.totalProtections += procedure.protectionCount || 0;
        });
        
        return stats;
    }

    // Debug method to check procedure data
    debugProcedures() {
        console.log('🐛 Procedure Debug Information:');
        console.log('Total procedures:', this.procedures.length);
        console.log('Data source:', window.apiClient.getDataSource());
        console.log('Procedures:', this.procedures);
        
        this.procedures.forEach(procedure => {
            console.log(`📋 ${procedure.procedure_code}:`);
            console.log(`  - Type: ${procedure.type}`);
            console.log(`  - Visible: ${procedure.isVisible}`);
            console.log(`  - Has geometry: ${!!procedure.geometry}`);
            console.log(`  - Segments: ${procedure.segmentCount || 0}`);
            console.log(`  - Protections: ${procedure.protectionCount || 0}`);
            
            if (procedure.geometry) {
                console.log(`  - Coordinates: ${procedure.geometry.geometry?.coordinates?.length || 0}`);
            }
        });
        
        const stats = this.getProcedureStats();
        console.log('📊 Procedure Statistics:', stats);
    }


    // Debug and testing methods
    debugState() {
        console.log('🐛 Map Manager Debug State:');
        console.log('Drone zones in memory:', this.droneZones.length);
        console.log('Drone zone layers on map:', this.layers.droneZones.getLayers().length);
        console.log('Current drawn layer:', !!this.currentDrawnLayer);
        console.log('Layer groups on map:', {
            procedures: this.map.hasLayer(this.layers.procedures),
            droneZones: this.map.hasLayer(this.layers.droneZones),
            conflicts: this.map.hasLayer(this.layers.conflicts),
            drawnItems: this.map.hasLayer(this.drawnItems)
        });
        
        this.droneZones.forEach((zone, index) => {
            console.log(`Zone ${index + 1}:`, {
                id: zone.id,
                operationId: zone.operationId,
                status: zone.status,
                hasGeometry: !!zone.geometry
            });
        });
    }

    testPersistence() {
        console.log('🧪 Testing drone zone persistence...');
        
        const testGeometry = {
            type: 'Feature',
            properties: { shapeType: 'test' },
            geometry: {
                type: 'Polygon',
                coordinates: [[
                    [-74.006, 40.712],
                    [-74.005, 40.712], 
                    [-74.005, 40.713],
                    [-74.006, 40.713],
                    [-74.006, 40.712]
                ]]
            }
        };
        
        const testData = {
            operationId: 'TEST-PERSIST-' + Date.now(),
            altitudeMin: 0,
            altitudeMax: 400,
            startTime: new Date().toISOString(),
            endTime: new Date(Date.now() + 3600000).toISOString(),
            status: 'Active'
        };
        
        console.log('🧪 Creating test zone...');
        this.createDroneZone(testGeometry, testData);
        
        setTimeout(() => {
            console.log('🧪 Test results after 1 second:');
            console.log('Zones in memory:', this.droneZones.length);
            console.log('Layers on map:', this.layers.droneZones.getLayers().length);
            
            // Verify the test zone exists
            const testZone = this.droneZones.find(z => z.operationId.startsWith('TEST-PERSIST'));
            if (testZone) {
                console.log('✅ Test zone found in memory:', testZone.operationId);
            } else {
                console.error('❌ Test zone NOT found in memory');
            }
            
            // Check if layer exists on map
            const layersOnMap = this.layers.droneZones.getLayers();
            const testLayer = layersOnMap.find(layer => {
                return layer._droneZone && layer._droneZone.operationId.startsWith('TEST-PERSIST');
            });
            
            if (testLayer) {
                console.log('✅ Test layer found on map');
            } else {
                console.error('❌ Test layer NOT found on map');
            }
        }, 1000);
    }

    clearAllDroneZones() {
        console.log('🧹 Clearing all drone zones...');
        this.droneZones = [];
        this.layers.droneZones.clearLayers();
        console.log('✅ All drone zones cleared');
    }

    forceRenderDroneZones() {
        console.log('🔄 Force rendering drone zones...');
        this.renderDroneZones();
    }
}

// Create singleton instance
const mapManager = new MapManager();

// Make it globally available for popup callbacks and debugging
window.mapManager = mapManager;

export default mapManager;