// API communication module with enhanced database connectivity
import authManager from './auth.js';

class ApiClient {
    constructor() {
        this.baseUrl = '/api';
        this.timeout = 10000;
        this.dataSource = 'unknown'; // Track data source for debugging
    }

    async request(endpoint, options = {}) {
        const url = `${this.baseUrl}${endpoint}`;
        
        const config = {
            method: 'GET',
            headers: {
                'Content-Type': 'application/json',
                ...options.headers
            },
            ...options
        };

        // Add authentication header
        const authHeader = authManager.getAuthHeader();
        if (authHeader) {
            config.headers.Authorization = authHeader;
        }

        try {
            const controller = new AbortController();
            const timeoutId = setTimeout(() => controller.abort(), this.timeout);

            console.log(`🔄 Making API request to: ${url}`);
            
            const response = await fetch(url, {
                ...config,
                signal: controller.signal
            });

            clearTimeout(timeoutId);

            // Handle authentication errors
            if (response.status === 401) {
                console.log('Unauthorized request, redirecting to login...');
                await authManager.login();
                return;
            }

            if (!response.ok) {
                const errorData = await response.json().catch(() => ({}));
                throw new Error(errorData.message || `HTTP ${response.status}: ${response.statusText}`);
            }

            const data = await response.json();
            console.log(`✅ API request successful: ${endpoint}`, data);
            return data;
            
        } catch (error) {
            if (error.name === 'AbortError') {
                throw new Error('Request timeout');
            }
            
            console.error(`❌ API request failed: ${endpoint}`, error);
            throw error;
        }
    }

    async getAirports(filters = {}) {
        console.log('🛩️ Fetching airports...');
        
        try {
            console.log('🔄 Attempting to fetch airports from database...');
            
            const queryParams = new URLSearchParams();
            
            if (filters.type) {
                queryParams.append('type', filters.type);
            }
            if (filters.active_only !== undefined) {
                queryParams.append('active_only', filters.active_only);
            }
            if (filters.country) {
                queryParams.append('country', filters.country);
            }
            if (filters.bounds) {
                // queryParams.append('min_lat', filters.bounds.min_lat);
                // queryParams.append('max_lat', filters.bounds.max_lat);
                // queryParams.append('min_lng', filters.bounds.min_lng);
                // queryParams.append('max_lng', filters.bounds.max_lng);
            }
            if (filters.limit) {
                queryParams.append('limit', filters.limit);
            }
            
            // const endpoint = filters.bounds ? 
            //     `/airports/bounds${queryParams.toString() ? '?' + queryParams.toString() : ''}` :
            //     `/airports${queryParams.toString() ? '?' + queryParams.toString() : ''}`;
            
                const endpoint =                 `/airports${queryParams.toString() ? '?' + queryParams.toString() : ''}`;

            const response = await this.request(endpoint);
            
            const airports = response.data || response;
            this.dataSource = 'database';
            
            console.log(`✅ Successfully fetched ${airports.length} airports from DATABASE`);
            
            // Process airports for map display
            const processedAirports = this.processAirportsForMap(airports);
            
            processedAirports._metadata = {
                source: 'database',
                timestamp: new Date().toISOString(),
                count: airports.length
            };
            
            return processedAirports;
            
        } catch (error) {
            console.warn(`⚠️ Database fetch failed for airports: ${error.message}`);
            
            console.log('🔄 Falling back to mock airports...');
            this.dataSource = 'mock';
            
            const mockAirports = this.getMockAirports();
            
            mockAirports._metadata = {
                source: 'mock',
                timestamp: new Date().toISOString(),
                count: mockAirports.length,
                reason: error.message
            };
            
            console.log('⚠️ Using mock airports due to database connection failure');
            return mockAirports;
        }
    }

    async getAirportByIcao(icaoCode) {
        console.log(`🛩️ Fetching airport by ICAO: ${icaoCode}`);
        
        try {
            console.log('🔄 Attempting to fetch from database...');
            const response = await this.request(`/airports/icao/${icaoCode}`);
            const airport = response.data || response;
            
            this.dataSource = 'database';
            console.log(`✅ Successfully fetched airport ${icaoCode} from DATABASE`, airport);
            
            return this.processAirportForMap(airport);
            
        } catch (error) {
            console.warn(`⚠️ Database fetch failed for airport ${icaoCode}: ${error.message}`);
            
            console.log('🔄 Falling back to mock data...');
            this.dataSource = 'mock';
            
            const mockAirports = this.getMockAirports();
            const airport = mockAirports.find(a => a.icao_code === icaoCode);
            
            if (airport) {
                console.log(`⚠️ Using mock airport ${icaoCode}`);
                return airport;
            } else {
                console.error(`❌ Airport ${icaoCode} not found in mock data either`);
                return null;
            }
        }
    }

    async searchAirports(query, limit = 20) {
        console.log(`🔍 Searching airports for: ${query}`);
        
        try {
            console.log('🔄 Attempting to search in database...');
            const queryParams = new URLSearchParams({
                q: query,
                limit: limit.toString()
            });
            
            const response = await this.request(`/airports/search?${queryParams.toString()}`);
            const airports = response.data || response;
            
            this.dataSource = 'database';
            console.log(`✅ Found ${airports.length} airports matching "${query}" from DATABASE`);
            
            return this.processAirportsForMap(airports);
            
        } catch (error) {
            console.warn(`⚠️ Database search failed for "${query}": ${error.message}`);
            
            console.log('🔄 Falling back to mock search...');
            this.dataSource = 'mock';
            
            const mockAirports = this.getMockAirports();
            const filtered = mockAirports.filter(a => 
                a.icao_code.toLowerCase().includes(query.toLowerCase()) ||
                a.iata_code.toLowerCase().includes(query.toLowerCase()) ||
                a.name.toLowerCase().includes(query.toLowerCase()) ||
                a.municipality.toLowerCase().includes(query.toLowerCase())
            ).slice(0, limit);
            
            console.log(`⚠️ Found ${filtered.length} mock airports matching "${query}"`);
            return filtered;
        }
    }

    async getAirportRunways(icaoCode) {
        console.log(`🛬 Fetching runways for airport: ${icaoCode}`);
        
        try {
            console.log('🔄 Attempting to fetch from database...');
            const response = await this.request(`/airports/runways/${icaoCode}`);
            const runways = response.data || response;
            
            this.dataSource = 'database';
            console.log(`✅ Successfully fetched ${runways.length} runways for ${icaoCode} from DATABASE`);
            
            return runways;
            
        } catch (error) {
            console.warn(`⚠️ Database fetch failed for runways of ${icaoCode}: ${error.message}`);
            
            console.log('🔄 Falling back to mock runways...');
            this.dataSource = 'mock';
            
            const mockRunways = this.getMockRunways(icaoCode);
            console.log(`⚠️ Using ${mockRunways.length} mock runways for ${icaoCode}`);
            return mockRunways;
        }
    }

    // Process airports for map display
    processAirportsForMap(airports) {
        console.log('🗺️ Processing airports for map display...');
        
        const processedAirports = airports.map(airport => this.processAirportForMap(airport));
        
        console.log(`✅ Processed ${processedAirports.length} airports for map`);
        return processedAirports;
    }

    processAirportForMap(airport) {
        return {
            id: airport.id,
            icao_code: airport.icao_code,
            iata_code: airport.iata_code,
            name: airport.name,
            full_name: airport.full_name,
            municipality: airport.municipality,
            country_code: airport.country_code,
            country_name: airport.country_name,
            airport_type: airport.airport_type,
            elevation_ft: airport.elevation_ft,
            isVisible: true,
            
            // Create geometry for map display
            geometry: {
                type: 'Feature',
                properties: {
                    icao_code: airport.icao_code,
                    iata_code: airport.iata_code,
                    name: airport.name,
                    airport_type: airport.airport_type,
                    elevation_ft: airport.elevation_ft,
                    has_tower: airport.has_tower,
                    has_ils: airport.has_ils,
                    runway_count: airport.runway_count
                },
                geometry: {
                    type: 'Point',
                    coordinates: [airport.longitude, airport.latitude]
                }
            }
        };
    }

    // Mock data for testing
    getMockAirports() {
        console.log('🧪 Generating mock airports...');
        
        const mockAirports = [
            {
                id: 1,
                icao_code: 'KJFK',
                iata_code: 'JFK',
                name: '[MOCK] John F Kennedy Intl',
                full_name: 'John F Kennedy International Airport',
                latitude: 40.639751,
                longitude: -73.778925,
                elevation_ft: 13,
                airport_type: 'large_airport',
                municipality: 'New York',
                region: 'US-NY',
                country_code: 'US',
                country_name: 'United States',
                is_active: true,
                has_tower: true,
                has_ils: true,
                runway_count: 4,
                longest_runway_ft: 14572
            },
            {
                id: 2,
                icao_code: 'KLGA',
                iata_code: 'LGA',
                name: '[MOCK] LaGuardia',
                full_name: 'LaGuardia Airport',
                latitude: 40.777245,
                longitude: -73.872608,
                elevation_ft: 21,
                airport_type: 'large_airport',
                municipality: 'New York',
                region: 'US-NY',
                country_code: 'US',
                country_name: 'United States',
                is_active: true,
                has_tower: true,
                has_ils: true,
                runway_count: 2,
                longest_runway_ft: 7003
            },
            {
                id: 3,
                icao_code: 'KEWR',
                iata_code: 'EWR',
                name: '[MOCK] Newark Liberty Intl',
                full_name: 'Newark Liberty International Airport',
                latitude: 40.692500,
                longitude: -74.168667,
                elevation_ft: 18,
                airport_type: 'large_airport',
                municipality: 'Newark',
                region: 'US-NJ',
                country_code: 'US',
                country_name: 'United States',
                is_active: true,
                has_tower: true,
                has_ils: true,
                runway_count: 3,
                longest_runway_ft: 11000
            }
        ];
        
        console.log(`🧪 Generated ${mockAirports.length} mock airports`);
        return mockAirports;
    }

    getMockRunways(icaoCode) {
        console.log(`🧪 Generating mock runways for ${icaoCode}...`);
        
        const runwayData = {
            'KJFK': [
                {
                    id: 1,
                    airport_id: 1,
                    runway_identifier: '04L/22R',
                    length_ft: 14572,
                    width_ft: 150,
                    surface_type: 'ASP',
                    le_ident: '04L',
                    le_heading_deg: 40.8,
                    he_ident: '22R',
                    he_heading_deg: 220.8,
                    is_active: true
                },
                {
                    id: 2,
                    airport_id: 1,
                    runway_identifier: '04R/22L',
                    length_ft: 8400,
                    width_ft: 150,
                    surface_type: 'ASP',
                    le_ident: '04R',
                    le_heading_deg: 40.8,
                    he_ident: '22L',
                    he_heading_deg: 220.8,
                    is_active: true
                }
            ],
            'KLGA': [
                {
                    id: 3,
                    airport_id: 2,
                    runway_identifier: '04/22',
                    length_ft: 7003,
                    width_ft: 150,
                    surface_type: 'ASP',
                    le_ident: '04',
                    le_heading_deg: 40.0,
                    he_ident: '22',
                    he_heading_deg: 220.0,
                    is_active: true
                }
            ],
            'KEWR': [
                {
                    id: 4,
                    airport_id: 3,
                    runway_identifier: '04L/22R',
                    length_ft: 11000,
                    width_ft: 150,
                    surface_type: 'ASP',
                    le_ident: '04L',
                    le_heading_deg: 40.0,
                    he_ident: '22R',
                    he_heading_deg: 220.0,
                    is_active: true
                }
            ]
        };
        
        const runways = runwayData[icaoCode] || [];
        console.log(`🧪 Generated ${runways.length} mock runways for ${icaoCode}`);
        return runways;
    }


    // Enhanced Projects API with database priority
    async getProjects(filters = {}) {
        console.log('📂 Fetching projects...');
        
        try {
            // Always try database first
            console.log('🔄 Attempting to fetch from database...');
            
            const queryParams = new URLSearchParams();
            
            if (filters.status) {
                queryParams.append('status', filters.status);
            }
            if (filters.demander_id) {
                queryParams.append('demander_id', filters.demander_id);
            }
            
            const endpoint = `/projects${queryParams.toString() ? '?' + queryParams.toString() : ''}`;
            const response = await this.request(endpoint);
            
            // Success - we got data from database
            const projects = response.data || response;
            this.dataSource = 'database';
            
            console.log(`✅ Successfully fetched ${projects.length} projects from DATABASE`);
            console.log('📊 Database projects:', projects);
            
            // Add metadata to help identify data source
            projects._metadata = {
                source: 'database',
                timestamp: new Date().toISOString(),
                count: projects.length
            };
            
            return projects;
            
        } catch (error) {
            console.warn(`⚠️ Database fetch failed: ${error.message}`);
            
            // Only use mock data as absolute fallback
            console.log('🔄 Falling back to mock data...');
            this.dataSource = 'mock';
            
            const mockProjects = this.getMockProjects();
            
            // Add metadata to help identify data source
            mockProjects._metadata = {
                source: 'mock',
                timestamp: new Date().toISOString(),
                count: mockProjects.length,
                reason: error.message
            };
            
            console.log('⚠️ Using mock data due to database connection failure');
            return mockProjects;
        }
    }

    async getProject(projectId) {
        console.log(`📂 Fetching project ID: ${projectId}`);
        
        try {
            console.log('🔄 Attempting to fetch from database...');
            const response = await this.request(`/projects/${projectId}`);
            const project = response.data || response;
            
            this.dataSource = 'database';
            console.log(`✅ Successfully fetched project ${projectId} from DATABASE`, project);
            
            return project;
            
        } catch (error) {
            console.warn(`⚠️ Database fetch failed for project ${projectId}: ${error.message}`);
            
            // Fallback to mock data
            console.log('🔄 Falling back to mock data...');
            this.dataSource = 'mock';
            
            const mockProjects = this.getMockProjects();
            const project = mockProjects.find(p => p.id == projectId);
            
            if (project) {
                console.log(`⚠️ Using mock project ${projectId}`);
                return project;
            } else {
                console.error(`❌ Project ${projectId} not found in mock data either`);
                return null;
            }
        }
    }

    async createProject(projectData) {
        console.log('📝 Creating new project with data:', projectData);
    
        // Map frontend camelCase names to backend snake_case names
        const payload = {
            title: projectData.title,
            description: projectData.description,
            start_date: projectData.startDate,
            end_date: projectData.endDate,
            altitude_min: projectData.altitudeMin,
            altitude_max: projectData.altitudeMax,
            priority: projectData.priority,
            operation_type: projectData.operationType,
            demander_id: projectData.demander_id,
            demander_name: projectData.demander_name,
            demander_organization: projectData.demander_organization,
            demander_email: projectData.demander_email
        };
    
        try {
            console.log('🔄 Attempting to create in database with payload:', payload);
            const response = await this.request('/projects', {
                method: 'POST',
                body: JSON.stringify(payload)
            });
    
            const project = response.data || response;
            this.dataSource = 'database';
    
            console.log('✅ Successfully created project in DATABASE:', project);
            return project;
    
        } catch (error) {
            console.error(`⚠️ Database create failed: ${error.message}`);
            // Log the full error to see backend validation messages
            console.error('Full error response:', error);
            
            // Re-throw the error so the UI can display a helpful message
            throw error;
        }
    }

    // Project Geometries API
    async getProjectGeometries(projectId) {
        console.log(`📐 Fetching geometries for project ${projectId}`);
        
        try {
            console.log('🔄 Attempting to fetch from database...');
            const response = await this.request(`/projects/${projectId}/geometries`);
            const geometries = response.data || response;
            
            this.dataSource = 'database';
            console.log(`✅ Successfully fetched ${geometries.length} geometries from DATABASE`);
            return geometries;
            
        } catch (error) {
            console.warn(`⚠️ Database fetch failed for geometries: ${error.message}`);
            console.log('🔄 Returning empty geometries array');
            return [];
        }
    }

    async createProjectGeometry(projectId, geometryData) {
        console.log(`📐 Creating geometry for project ${projectId}`, geometryData);
        
        try {
            console.log('🔄 Attempting to create in database...');
            const response = await this.request(`/projects/${projectId}/geometries`, {
                method: 'POST',
                body: JSON.stringify(geometryData)
            });
            
            const geometry = response.data || response;
            this.dataSource = 'database';
            
            console.log('✅ Successfully created geometry in DATABASE:', geometry);
            return geometry;
            
        } catch (error) {
            console.warn(`⚠️ Database create failed for geometry: ${error.message}`);
            
            // Fallback to mock creation
            console.log('🔄 Creating mock geometry...');
            this.dataSource = 'mock';
            
            const mockGeometry = this.createMockGeometry(projectId, geometryData);
            console.log('⚠️ Created mock geometry:', mockGeometry);
            return mockGeometry;
        }
    }

    // Flight Procedures API
    async getProcedures(filters = {}) {
        console.log('✈️ Fetching flight procedures from database...');
        
        try {
            console.log('🔄 Attempting to fetch procedures from database...');
            
            const queryParams = new URLSearchParams();
            
            // Add filters
            if (filters.type) queryParams.append('type', filters.type);
            if (filters.airport_icao) queryParams.append('airport_icao', filters.airport_icao);
            if (filters.runway) queryParams.append('runway', filters.runway);
            if (filters.is_active !== undefined) queryParams.append('is_active', filters.is_active);
            
            // Always include detailed data for map display
            // queryParams.append('include_segments', 'true');
            // queryParams.append('include_protections', 'true');
            
            if (filters.limit) queryParams.append('limit', filters.limit);
            if (filters.offset) queryParams.append('offset', filters.offset);
            
            const endpoint = `/procedures${queryParams.toString() ? '?' + queryParams.toString() : ''}`;
            const response = await this.request(endpoint);
            
            // Success - we got data from database
            const procedures = response.data || response;
            this.dataSource = 'database';
            
            console.log(`✅ Successfully fetched ${procedures.length} procedures from DATABASE`);
            console.log('✈️ Database procedures:', procedures);
            
            // Process procedures for map display
            const processedProcedures = this.processProceduresForMap(procedures);
            
            // Add metadata to help identify data source
            processedProcedures._metadata = {
                source: 'database',
                timestamp: new Date().toISOString(),
                count: procedures.length
                // ,
                // withSegments: procedures.some(p => p.segments && p.segments.length > 0),
                // withProtections: procedures.some(p => p.protections && p.protections.length > 0)
            };
            
            return processedProcedures;
            
        } catch (error) {
            console.warn(`⚠️ Database fetch failed: ${error.message}`);
            
            // Fallback to mock data
            console.log('🔄 Falling back to mock procedures...');
            this.dataSource = 'mock';
            
            const mockProcedures = this.getMockProcedures();
            
            // Add metadata to help identify data source
            mockProcedures._metadata = {
                source: 'mock',
                timestamp: new Date().toISOString(),
                count: mockProcedures.length,
                reason: error.message
            };
            
            console.log('⚠️ Using mock procedures due to database connection failure');
            return mockProcedures;
        }
    }

    async getProcedure(procedureId) {
        console.log(`✈️ Fetching procedure ID: ${procedureId}`);
        
        try {
            console.log('🔄 Attempting to fetch from database...');
            const response = await this.request(`/procedures/${procedureId}?include_segments=true&include_protections=true`);
            const procedure = response.data || response;
            
            this.dataSource = 'database';
            console.log(`✅ Successfully fetched procedure ${procedureId} from DATABASE`, procedure);
            
            // Process for map display
            return this.processProcedureForMap(procedure);
            
        } catch (error) {
            console.warn(`⚠️ Database fetch failed for procedure ${procedureId}: ${error.message}`);
            
            // Fallback to mock data
            console.log('🔄 Falling back to mock data...');
            this.dataSource = 'mock';
            
            const mockProcedures = this.getMockProcedures();
            const procedure = mockProcedures.find(p => p.id == procedureId);
            
            if (procedure) {
                console.log(`⚠️ Using mock procedure ${procedureId}`);
                return procedure;
            } else {
                console.error(`❌ Procedure ${procedureId} not found in mock data either`);
                return null;
            }
        }
    }

    async getProceduresByAirport(airportIcao) {
        console.log(`✈️ Fetching procedures for airport: ${airportIcao}`);
        
        try {
            console.log('🔄 Attempting to fetch from database...');
            const response = await this.request(`/procedures/airport/${airportIcao}?include_segments=true&include_protections=true`);
            const procedures = response.data || response;
            
            this.dataSource = 'database';
            console.log(`✅ Successfully fetched ${procedures.length} procedures for ${airportIcao} from DATABASE`);
            
            // Process procedures for map display
            return this.processProceduresForMap(procedures);
            
        } catch (error) {
            console.warn(`⚠️ Database fetch failed for airport ${airportIcao}: ${error.message}`);
            
            // Fallback to mock data filtered by airport
            console.log('🔄 Falling back to mock data...');
            this.dataSource = 'mock';
            
            const mockProcedures = this.getMockProcedures();
            const filteredProcedures = mockProcedures.filter(p => 
                p.airport_icao === airportIcao || 
                p.geometry?.properties?.airport_icao === airportIcao
            );
            
            console.log(`⚠️ Using ${filteredProcedures.length} mock procedures for ${airportIcao}`);
            return filteredProcedures;
        }
    }

    // Conflicts API
    async getConflicts(projectId = null) {
        console.log('⚠️ Fetching conflicts...');
        
        try {
            console.log('🔄 Attempting to fetch from database...');
            const endpoint = projectId ? `/projects/${projectId}/conflicts` : '/conflicts';
            const response = await this.request(endpoint);
            const conflicts = response.data || response;
            
            this.dataSource = 'database';
            console.log(`✅ Successfully fetched ${conflicts.length} conflicts from DATABASE`);
            return conflicts;
            
        } catch (error) {
            console.warn(`⚠️ Database fetch failed for conflicts: ${error.message}`);
            console.log('🔄 Returning empty conflicts array');
            return [];
        }
    }

    // Enhanced health check with detailed logging
    async healthCheck() {
        try {
            console.log('🏥 Performing backend health check...');
            const response = await this.request('/health');
            
            console.log('✅ Backend health check passed:', response);
            return true;
            
        } catch (error) {
            console.warn('❌ Backend health check failed:', error.message);
            return false;
        }
    }

    // Get current data source for debugging
    getDataSource() {
        return this.dataSource;
    }

    // Check if we're using real database data
    isUsingDatabase() {
        return this.dataSource === 'database';
    }

    // Mock data methods (unchanged but with better logging)
    getMockProjects() {
        const currentUser = authManager.getUser();
        console.log('🧪 Generating mock projects...');
        
        const mockProjects = [
            {
                id: 1,
                project_code: 'MOCK-2025-001',
                title: '[MOCK] Downtown Construction Survey',
                description: 'Aerial survey for new building construction project',
                demander_name: currentUser?.fullName || 'Test User',
                demander_organization: 'Construction Corp',
                demander_email: currentUser?.email || 'test@construction.com',
                status: 'Created',
                priority: 'Normal',
                operation_type: 'Surveying',
                altitude_min: 50,
                altitude_max: 400,
                start_date: '2025-08-15T09:00:00',
                end_date: '2025-08-15T17:00:00',
                created_at: '2025-08-11T10:00:00Z',
                geometry_count: 2,
                conflict_count: 1
            },
            {
                id: 2,
                project_code: 'MOCK-2025-002',
                title: '[MOCK] Real Estate Photography',
                description: 'Aerial photography for property marketing',
                demander_name: currentUser?.fullName || 'Jane Smith',
                demander_organization: 'Real Estate LLC',
                demander_email: currentUser?.email || 'jane@realestate.com',
                status: 'Pending',
                priority: 'Low',
                operation_type: 'Aerial Photography',
                altitude_min: 100,
                altitude_max: 350,
                start_date: '2025-08-16T14:00:00',
                end_date: '2025-08-16T16:00:00',
                created_at: '2025-08-11T11:00:00Z',
                geometry_count: 1,
                conflict_count: 0
            },
            {
                id: 3,
                project_code: 'MOCK-2025-003',
                title: '[MOCK] Infrastructure Inspection',
                description: 'Bridge inspection using thermal imaging',
                demander_name: currentUser?.fullName || 'Mike Johnson',
                demander_organization: 'Infrastructure Corp',
                demander_email: currentUser?.email || 'mike@infrastructure.com',
                status: 'Under_Review',
                priority: 'High',
                operation_type: 'Inspection',
                altitude_min: 75,
                altitude_max: 500,
                start_date: '2025-08-18T08:00:00',
                end_date: '2025-08-18T12:00:00',
                created_at: '2025-08-11T12:00:00Z',
                geometry_count: 2,
                conflict_count: 1
            }
        ];
        
        console.log(`🧪 Generated ${mockProjects.length} mock projects`);
        return mockProjects;
    }


    // Process database procedures for map display
processProceduresForMap(procedures) {
    console.log('🗺️ Processing procedures for map display...');
    
    const processedProcedures = procedures.map(procedure => this.processProcedureForMap(procedure));
    
    console.log(`✅ Processed ${processedProcedures.length} procedures for map`);
    return processedProcedures;
}

// processProcedureForMap(procedure) {
//     console.log(`🗺️ Processing procedure ${procedure.procedure_code} for map...`);
    
//     try {
//         // Create the base procedure object
//         const mapProcedure = {
//             id: procedure.id,
//             procedure_code: procedure.procedure_code,
//             name: procedure.name,
//             type: procedure.type,
//             airport_icao: procedure.airport_icao,
//             runway: procedure.runway,
//             description: procedure.description,
//             is_active: procedure.is_active,
//             isVisible: true, // Default to visible
            
//             // Altitude constraints from segments
//             altitudeConstraints: this.extractAltitudeConstraints(procedure.segments),
            
//             // Main geometry - combine all segments into a single LineString
//             geometry: this.createProcedureGeometry(procedure),
            
//             // Individual segments for detailed display
//             segments: procedure.segments || [],
            
//             // Protection areas
//             protections: procedure.protections || [],
            
//             // Metadata
//             segmentCount: procedure.segments?.length || 0,
//             protectionCount: procedure.protections?.length || 0
//         };
        
//         console.log(`✅ Processed ${mapProcedure.procedure_code}: ${mapProcedure.segmentCount} segments, ${mapProcedure.protectionCount} protections`);
//         return mapProcedure;
        
//     } catch (error) {
//         console.error(`❌ Error processing procedure ${procedure.procedure_code}:`, error);
        
//         // Return a basic procedure object if processing fails
//         return {
//             id: procedure.id,
//             procedure_code: procedure.procedure_code,
//             name: procedure.name,
//             type: procedure.type,
//             airport_icao: procedure.airport_icao,
//             isVisible: true,
//             geometry: null,
//             segments: [],
//             protections: [],
//             error: error.message
//         };
//     }
// }

// Much simpler - no segments/protections processing needed
processProcedureForMap(procedure) {
    return {
        id: procedure.id,
        procedure_code: procedure.procedure_code,
        name: procedure.name,
        type: procedure.type,
        // Direct GeoJSON - no processing needed
        geometry: procedure.trajectory_geometry ? 
                  JSON.parse(procedure.trajectory_geometry) : null,
        protections: procedure.protection_geometry ? 
                    JSON.parse(procedure.protection_geometry) : null
    };
}

createProcedureGeometry(procedure) {
    console.log(`📐 Creating geometry for ${procedure.procedure_code}...`);
    
    try {
        if (!procedure.segments || procedure.segments.length === 0) {
            console.warn(`⚠️ No segments found for ${procedure.procedure_code}`);
            return null;
        }
        
        // Combine all segment trajectories into a single LineString
        const allCoordinates = [];
        const sortedSegments = procedure.segments.sort((a, b) => a.segment_order - b.segment_order);
        
        sortedSegments.forEach((segment, index) => {
            try {
                let segmentGeometry;
                
                // Parse the trajectory geometry
                if (typeof segment.trajectory_geometry === 'string') {
                    segmentGeometry = JSON.parse(segment.trajectory_geometry);
                } else {
                    segmentGeometry = segment.trajectory_geometry;
                }
                
                if (segmentGeometry && segmentGeometry.coordinates) {
                    const coords = segmentGeometry.coordinates;
                    
                    if (index === 0) {
                        // First segment - add all coordinates
                        allCoordinates.push(...coords);
                    } else {
                        // Subsequent segments - skip first coordinate to avoid duplication
                        allCoordinates.push(...coords.slice(1));
                    }
                    
                    console.log(`📍 Added ${coords.length} coordinates from segment ${segment.segment_order}`);
                } else {
                    console.warn(`⚠️ Invalid geometry for segment ${segment.segment_order}`);
                }
                
            } catch (error) {
                console.error(`❌ Error parsing segment ${segment.segment_order} geometry:`, error);
            }
        });
        
        if (allCoordinates.length === 0) {
            console.warn(`⚠️ No valid coordinates found for ${procedure.procedure_code}`);
            return null;
        }
        
        // Create GeoJSON Feature
        const geometry = {
            type: 'Feature',
            properties: {
                procedure_code: procedure.procedure_code,
                name: procedure.name,
                type: procedure.type,
                airport_icao: procedure.airport_icao,
                runway: procedure.runway,
                segment_count: sortedSegments.length
            },
            geometry: {
                type: 'LineString',
                coordinates: allCoordinates
            }
        };
        
        console.log(`✅ Created geometry for ${procedure.procedure_code}: ${allCoordinates.length} coordinates`);
        return geometry;
        
    } catch (error) {
        console.error(`❌ Error creating geometry for ${procedure.procedure_code}:`, error);
        return null;
    }
}

extractAltitudeConstraints(segments) {
    if (!segments || segments.length === 0) {
        return { min: null, max: null };
    }
    
    let minAlt = null;
    let maxAlt = null;
    
    segments.forEach(segment => {
        if (segment.altitude_min !== null && segment.altitude_min !== undefined) {
            minAlt = minAlt === null ? segment.altitude_min : Math.min(minAlt, segment.altitude_min);
        }
        if (segment.altitude_max !== null && segment.altitude_max !== undefined) {
            maxAlt = maxAlt === null ? segment.altitude_max : Math.max(maxAlt, segment.altitude_max);
        }
    });
    
    return { min: minAlt, max: maxAlt };
}





   // Enhanced mock procedures with more realistic data
getMockProcedures() {
    console.log('✈️ Generating enhanced mock procedures...');
    
    const mockProcedures = [
        {
            id: 'mock_001',
            procedure_code: 'MOCK-CANDR6',
            name: '[MOCK] CANDR6 Departure',
            type: 'SID',
            runway: '04L',
            airport_icao: 'KJFK',
            isVisible: true,
            altitudeConstraints: { min: 1000, max: 18000 },
            geometry: {
                type: 'Feature',
                properties: {
                    procedure_code: 'MOCK-CANDR6',
                    name: '[MOCK] CANDR6 Departure',
                    type: 'SID'
                },
                geometry: {
                    type: 'LineString',
                    coordinates: [
                        [-73.778925, 40.639751], // JFK
                        [-73.750000, 40.680000], // CANDR
                        [-73.700000, 40.750000], // BEWEL
                        [-73.650000, 40.820000]  // ZENDA
                    ]
                }
            },
            segments: [
                {
                    id: 1,
                    segment_order: 1,
                    segment_name: 'Initial Climb',
                    waypoint_from: 'JFK',
                    waypoint_to: 'CANDR',
                    altitude_min: 1000,
                    altitude_max: 3000
                },
                {
                    id: 2,
                    segment_order: 2,
                    segment_name: 'Continue Climb',
                    waypoint_from: 'CANDR',
                    waypoint_to: 'BEWEL',
                    altitude_min: 3000,
                    altitude_max: 10000
                }
            ],
            protections: [],
            segmentCount: 2,
            protectionCount: 0
        },
        {
            id: 'mock_002',
            procedure_code: 'MOCK-LENDY4',
            name: '[MOCK] LENDY4 Arrival',
            type: 'STAR',
            runway: '04L',
            airport_icao: 'KJFK',
            isVisible: true,
            altitudeConstraints: { min: 3000, max: 10000 },
            geometry: {
                type: 'Feature',
                properties: {
                    procedure_code: 'MOCK-LENDY4',
                    name: '[MOCK] LENDY4 Arrival',
                    type: 'STAR'
                },
                geometry: {
                    type: 'LineString',
                    coordinates: [
                        [-73.400000, 41.200000], // LENDY
                        [-73.500000, 41.100000],
                        [-73.600000, 41.000000],
                        [-73.700000, 40.900000],
                        [-73.778925, 40.639751]  // JFK
                    ]
                }
            },
            segments: [
                {
                    id: 3,
                    segment_order: 1,
                    segment_name: 'Initial Descent',
                    waypoint_from: 'LENDY',
                    waypoint_to: 'BEWEL',
                    altitude_min: 8000,
                    altitude_max: 10000
                }
            ],
            protections: [],
            segmentCount: 1,
            protectionCount: 0
        }
    ];
    
    console.log(`✈️ Generated ${mockProcedures.length} mock procedures`);
    return mockProcedures;
}

    createMockProject(projectData) {
        const currentUser = authManager.getUser();
        const timestamp = Date.now();
        
        const mockProject = {
            id: timestamp,
            project_code: `MOCK-${new Date().getFullYear()}-${String(timestamp).slice(-3)}`,
            title: `[MOCK] ${projectData.title}`,
            description: projectData.description || '',
            demander_name: projectData.demanderName,
            demander_organization: projectData.demanderOrganization || '',
            demander_email: projectData.demanderEmail,
            demander_phone: projectData.demanderPhone || '',
            status: 'Created',
            priority: projectData.priority,
            operation_type: projectData.operationType,
            altitude_min: parseInt(projectData.altitudeMin),
            altitude_max: parseInt(projectData.altitudeMax),
            start_date: projectData.startDate,
            end_date: projectData.endDate,
            created_at: new Date().toISOString(),
            demander_id: currentUser?.id || 1,
            geometry_count: 0,
            conflict_count: 0
        };
        
        console.log('🧪 Created mock project:', mockProject);
        return mockProject;
    }

    createMockGeometry(projectId, geometryData) {
        const timestamp = Date.now();
        
        const mockGeometry = {
            id: timestamp,
            project_id: projectId,
            geometry_type: geometryData.geometryType,
            name: geometryData.name,
            description: geometryData.description || '',
            geometry_data: geometryData.geometry,
            center_lat: geometryData.centerLat || null,
            center_lng: geometryData.centerLng || null,
            area_size: geometryData.areaSize || null,
            altitude_min: geometryData.altitudeMin || null,
            altitude_max: geometryData.altitudeMax || null,
            is_primary: geometryData.isPrimary || false,
            created_at: new Date().toISOString(),
            updated_at: new Date().toISOString()
        };
        
        console.log('🧪 Created mock geometry:', mockGeometry);
        return mockGeometry;
    }

    // Utility method to generate project code
    generateProjectCode() {
        const year = new Date().getFullYear();
        const timestamp = Date.now().toString().slice(-6);
        return `PROJ-${year}-${timestamp}`;
    }

    async submitProject(projectId, geometries) {
        const endpoint = `/projects/${projectId}/submit`;
        console.log(`🚀 Submitting project ${projectId} with ${geometries.length} geometries...`);
        

        const payload = {
            // Key is now singular: "geometry"
            geometry: {
                // This is the required FeatureCollection structure
                type: "FeatureCollection",
                features: geometries.map(zone => zone.geometry)
            }
        };
    
        try {
            // We use 'POST' to send data to the backend for processing
            const result = await this.request(endpoint, {
                method: 'POST',
                body: JSON.stringify(payload)
            });
            
            console.log('✅ Project submitted successfully, status received:', result);
            return this.pollForAnalysisCompletion(projectId);
    
        } catch (error) {
            console.error(`❌ Failed to submit project ${projectId}:`, error);
            // Re-throw the error so the UI can handle it
            throw error;
        }
    }

    // --- NEW POLLING FUNCTION ---
    async pollForAnalysisCompletion(projectId, timeout = 60000, interval = 2000) {
        const startTime = Date.now();

        return new Promise((resolve, reject) => {
            const checkStatus = async () => {
                // Check for timeout
                if (Date.now() - startTime > timeout) {
                    clearInterval(intervalId);
                    reject(new Error('Analysis timed out. Please check the project status later.'));
                    return;
                }

                try {
                    const project = await this.getProject(projectId);
                    console.log(`Polling... Project ${projectId} status is: ${project.status}`);

                    // The analysis is done when the status is no longer "Pending"
                    if (project.status !== 'Pending') {
                        clearInterval(intervalId);
                        console.log(`✅ Analysis complete! Status: ${project.status}`);

                        // Fetch the final conflicts
                        const conflicts = await this.getConflicts(projectId);

                        // Resolve with the complete report data
                        resolve({
                            project: project,
                            conflicts: conflicts
                        });
                    }
                } catch (error) {
                    clearInterval(intervalId);
                    reject(error);
                }
            };

            const intervalId = setInterval(checkStatus, interval);
        });
    }

    async getProjectGeometries(projectId) {
        console.log(`🗺️ Fetching geometries for project ${projectId}...`);
        try {
            const response = await this.request(`/projects/${projectId}/geometries`);
            // The response is already the FeatureCollection object
            return response;
        } catch (error) {
            console.error(`❌ Failed to fetch geometries for project ${projectId}:`, error);
            // Return an empty collection on error so the app doesn't crash
            return { type: "FeatureCollection", features: [] };
        }
    }
    
}

// Create singleton instance
const apiClient = new ApiClient();

// Add global debugging helpers
window.apiClient = apiClient;
window.debugApi = {
    getDataSource: () => apiClient.getDataSource(),
    isUsingDatabase: () => apiClient.isUsingDatabase(),
    testConnection: () => apiClient.healthCheck(),
    getProjects: () => apiClient.getProjects(),
    getAirports: () => apiClient.getAirports(),
    searchAirports: (query) => apiClient.searchAirports(query),
    getAirportRunways: (icao) => apiClient.getAirportRunways(icao)
};

export default apiClient;