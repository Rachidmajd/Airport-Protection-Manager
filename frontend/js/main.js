// Main application entry point - FIXED VERSION
import authManager from './auth.js';
import mapManager from './map.js';
import apiClient from './api.js';
import coordinateEntryManager from './coordinateEntry.js';
import uiManager, { 
    showLoading, 
    showLoginScreen, 
    showApp, 
    updateUserInfo,
    renderProcedureControls,
    renderAirportControls,
    showNotification,
    updateProjectBar,
    renderProjectList,
    hideProjectListModal
} from './ui.js';

class AeronauticalApp {
    constructor() {
        this.initialized = false;
        this.activeProject = null;
        this.projects = [];
    }

    // Update the main init method to include debugging
async init() {
    try {
        console.log('🚀 Initializing Aeronautical Deconfliction Platform...');
        
        // Setup debugging first
        this.setupGlobalDebugging();
        
        // CRITICAL FIX: Initialize UI first and await it
        console.log('🔄 Initializing UI Manager...');
        await uiManager.init();
        console.log('✅ UI Manager initialized');
        
        // Show loading screen
        showLoading(true);
        
        // Add a small delay to ensure loading screen is visible
        await new Promise(resolve => setTimeout(resolve, 1000));
        
        console.log('🔄 Initializing authentication...');
        const isAuthenticated = await authManager.init();
        console.log('🔑 Authentication result:', isAuthenticated);
        
        if (isAuthenticated) {
            console.log('✅ User is authenticated, initializing app...');
            await this.initializeAuthenticatedApp();
        } else {
            console.log('🔐 User not authenticated, showing login...');
            this.showLoginFlow();
        }
        
        this.initialized = true;
        console.log('🎉 Application initialized successfully');
        
        // Show debugging tip
        console.log('💡 Debug tip: Use window.debugApp.state() to check application state');
        
    } catch (error) {
        console.error('❌ Failed to initialize application:', error);
        console.error('Error stack:', error.stack);
        this.showError('Failed to initialize application. Please refresh the page.');
    }
}

async createNewProject(projectData) {
    try {
        const user = authManager.getUser();
        if (user) {
            // Add all necessary demander fields from the user object
            projectData.demander_id = 1;
            projectData.demander_name = user.fullName;
            projectData.demander_organization = user.organization;
            projectData.demander_email = user.email;
        }

        showLoading(true);
        const newProject = await apiClient.createProject(projectData);

        if (newProject && newProject.id) {
            await this.loadProjects();
            await this.setActiveProject(newProject.id);
            showNotification('Project created and opened successfully!', 'success');
        } else {
            // Use a more specific error message if available
            const errorMessage = newProject?.message || 'Failed to create project. Check console for details.';
            showNotification(errorMessage, 'error');
        }
    } catch (error) {
        console.error('Error creating project:', error);
        showNotification(`Error: ${error.message}`, 'error');
    } finally {
        showLoading(false);
    }
}

async refreshAllData() {
    try {
        console.log('🔄 Refreshing all application data...');
        
        // Refresh projects
        await this.loadProjects();
        
        // Refresh procedures
        const refreshPromises = [];
            
        if (mapManager.refreshProcedures) {
            refreshPromises.push(mapManager.refreshProcedures());
        }

        if (mapManager.loadAirportsFromDatabase) {
            refreshPromises.push(mapManager.loadAirportsFromDatabase());
        }

        if (mapManager.loadWaypointsFromDatabase) {
            refreshPromises.push(mapManager.loadWaypointsFromDatabase());
        }

        await Promise.all(refreshPromises);
        
        // Refresh procedure controls
        await this.loadProcedureControls();
        await this.loadAirportControls(); 
        await this.loadWaypointControls();
        
        showNotification('All data refreshed successfully', 'success');
        console.log('✅ All data refresh complete');
        
    } catch (error) {
        console.error('❌ Failed to refresh data:', error);
        showNotification('Failed to refresh some data', 'warning');
    }
}

    async loadWaypointControls() {
        try {
            console.log('🎯 Loading waypoint controls...');
            
            // Wait for map to be initialized
            let attempts = 0;
            const maxAttempts = 20;
            
            while (attempts < maxAttempts) {
                if (mapManager.map && mapManager.waypoints) {
                    console.log('✅ Map and waypoints ready, rendering controls');
                    
                    // Get waypoint statistics
                    const stats = mapManager.getWaypointStats ? mapManager.getWaypointStats() : null;
                    if (stats) {
                        console.log('📊 Waypoint statistics:', stats);
                    }
                    
                    // Render waypoint controls
                    uiManager.renderWaypointControls(mapManager.waypoints);
                    return;
                }
                
                console.log(`⏳ Waiting for waypoints... attempt ${attempts + 1}/${maxAttempts}`);
                await new Promise(resolve => setTimeout(resolve, 500));
                attempts++;
            }
            
            console.log('⚠️ Waypoints not loaded after waiting, using empty list');
            uiManager.renderWaypointControls([]);
            
        } catch (error) {
            console.error('❌ Failed to load waypoint controls:', error);
            uiManager.renderWaypointControls([]);
        }
    }

    // Add this method to the initializeAuthenticatedApp function
    async initializeAuthenticatedApp() {
        try {
            console.log('🔄 Setting up authenticated application...');
            
            // Hide loading screen and show app
            showLoading(false);
            showApp(true);
            
            const user = authManager.getUser();
            console.log('👤 Current user:', user);
            updateUserInfo(user);

            coordinateEntryManager.init();

            
            // Initialize map
            console.log('🗺️ Initializing map...');
            mapManager.init('map');
            
            // Setup event handlers
            this.setupLogoutHandler();
            this.setupConflictRefresh();

            this.setupProjectSubmissionHandler();
            
            // Load data in the correct order
            console.log('📊 Loading initial data...');
            
            // First load projects
            await this.loadProjects();
            
            // Then load procedures (map will handle this internally)
            console.log('✈️ Map will load procedures from database...');
            
            // Wait a bit for map to initialize procedures, then load controls
            setTimeout(async () => {
                await this.loadProcedureControls();
                await this.loadAirportControls();
                await this.loadWaypointControls();
            }, 2000);

            // Initialize project bar
            updateProjectBar(null);

            
            showNotification(`Welcome, ${user.firstName || user.username}!`, 'success');
            console.log('✅ Authenticated app initialization complete');
            
        } catch (error) {
            console.error('❌ Failed to initialize authenticated app:', error);
            this.showError('Failed to load application data.');
        }
    }

    // Add debug methods for troubleshooting
debugApplicationState() {
    console.log('🐛 Application Debug State:');
    console.log('='.repeat(50));
    
    // Check API client
    if (window.apiClient) {
        console.log('📡 API Client:');
        console.log('  - Data source:', window.apiClient.getDataSource());
        console.log('  - Using database:', window.apiClient.isUsingDatabase());
    }
    
    // Check map manager
    if (window.mapManager) {
        console.log('🗺️ Map Manager:');
        console.log('  - Map initialized:', !!window.mapManager.map);
        console.log('  - Procedures loaded:', window.mapManager.procedures?.length || 0);
        console.log('  - Waypoints loaded:', window.mapManager.waypoints?.length || 0);
        console.log('  - Drone zones:', window.mapManager.droneZones?.length || 0);
        
        if (window.mapManager.getProcedureStats) {
            const stats = window.mapManager.getProcedureStats();
            console.log('  - Procedure stats:', stats);
        }

        if (window.mapManager.getAirportStats) {
            const airportStats = window.mapManager.getAirportStats();
            console.log('  - Airport stats:', airportStats);
        }

        if (window.mapManager.getWaypointStats) {
            const waypointStats = window.mapManager.getWaypointStats();
            console.log('  - Waypoint stats:', waypointStats);
        }
    }
    
    // Check projects
    console.log('📂 Projects:');
    console.log('  - Loaded projects:', this.projects?.length || 0);
    console.log('  - Active project:', this.activeProject?.title || 'None');
    
    // Check UI state
    console.log('🖼️ UI State:');
    const procedureControls = document.getElementById('procedure-controls');
    const airportControls = document.getElementById('airport-controls');
    const waypointControls = document.getElementById('waypoint-controls');
    console.log('  - Procedure controls:', !!procedureControls);
    if (procedureControls) {
        console.log('  - Control content length:', procedureControls.innerHTML.length);
    }
    if (airportControls) {  
        console.log('  - Airport control content length:', airportControls.innerHTML.length);
    }
    
    console.log('='.repeat(50));
}

    // Add global debugging helpers
    setupGlobalDebugging() {
        // Make app instance globally available for debugging
        window.aeronauticalApp = this;
        
        // Add global debug commands
        window.debugApp = {
            // Show application state
            state: () => this.debugApplicationState(),
            
            // Refresh all data including airports
            refresh: () => this.refreshAllData(),
            
            // Enhanced API connectivity test
            testApi: async () => {
                console.log('🧪 Testing API connectivity (enhanced)...');
                try {
                    const health = await window.apiClient.healthCheck();
                    console.log('Health check:', health ? '✅ OK' : '❌ Failed');
                    
                    const projects = await window.apiClient.getProjects();
                    console.log('Projects:', projects.length, 'loaded');
                    
                    const procedures = await window.apiClient.getProcedures();
                    console.log('Procedures:', procedures.length, 'loaded');
                    
                    const airports = await window.apiClient.getAirports();
                    console.log('Airports:', airports.length, 'loaded');
                    
                    const waypoints = await window.apiClient.getWaypoints();
                    console.log('Waypoints:', waypoints.length, 'loaded');
                

                    return { 
                        health, 
                        projects: projects.length, 
                        procedures: procedures.length,
                        airports: airports.length,
                        waypoints: waypoints.length
                    };
                } catch (error) {
                    console.error('API test failed:', error);
                    return { error: error.message };
                }
            },
            
            // Enhanced map state check
            map: () => {
                if (window.mapManager) {
                    window.mapManager.debugProcedures();
                    
                    if (window.mapManager.debugAirports) {
                        window.mapManager.debugAirports();
                    }

                    if (window.mapManager.debugWaypoints) {
                        window.mapManager.debugWaypoints();
                    }
                    
                    const procStats = window.mapManager.getProcedureStats();
                    const airportStats = window.mapManager.getAirportStats ? 
                                    window.mapManager.getAirportStats() : null;  // NEW
                    
                    return { 
                        procedures: procStats, 
                        airports: airportStats,
                        waypoints: waypointStats
                    };
                }
                return 'Map not initialized';
            },
            
            // Force procedure reload
            reloadProcedures: async () => {
                if (window.mapManager && window.mapManager.refreshProcedures) {
                    await window.mapManager.refreshProcedures();
                    await this.loadProcedureControls();
                    return 'Procedures reloaded';
                }
                return 'Map manager not available';
            },
            
            // NEW: Force airport reload
            reloadAirports: async () => {
                if (window.mapManager && window.mapManager.loadAirportsFromDatabase) {
                    await window.mapManager.loadAirportsFromDatabase();
                    await this.loadAirportControls();
                    return 'Airports reloaded';
                }
                return 'Map manager not available';
            },

            reloadWaypoints: async () => {
                if (window.mapManager && window.mapManager.loadWaypointsFromDatabase) {
                    await window.mapManager.loadWaypointsFromDatabase();
                    await this.loadWaypointControls();
                    return 'Waypoints reloaded';
                }
                return 'Map manager not available';
            },
            
            // NEW: Search airports
            searchAirports: async (query) => {
                if (window.mapManager && window.mapManager.searchAirports) {
                    return await window.mapManager.searchAirports(query);
                }
                return 'Map manager not available';
            },
            
            // NEW: Focus on airport
            focusAirport: (icao) => {
                if (window.mapManager && window.mapManager.focusOnAirport) {
                    window.mapManager.focusOnAirport(icao);
                    return `Focused on ${icao}`;
                }
                return 'Map manager not available';
            }
        };
        
        console.log('🐛 Enhanced global debugging available via window.debugApp');
        console.log('💡 Try: debugApp.state(), debugApp.testApi(), debugApp.searchAirports("KJFK")');
    }
    
    async loadProjects() {
        try {
            console.log('📂 Loading projects...');
            this.projects = await apiClient.getProjects();
            console.log(`✅ Loaded ${this.projects.length} projects`);
            renderProjectList(this.projects);
        } catch (error) {
            console.error('❌ Failed to load projects:', error);
            showNotification('Failed to load projects', 'error');
            renderProjectList([]);
        }
    }

    async setActiveProject(projectId) {
        console.log(`🚀 Activating project: ${projectId}`);
        const project = this.projects.find(p => p.id == projectId);
    
        if (project) {
            this.activeProject = project;
            updateProjectBar(this.activeProject);
            hideProjectListModal(); // Hide the modal first for better UX
    
            showLoading(true); // Now show the spinner
            try {
                const geometryCollection = await apiClient.getProjectGeometries(projectId);
                mapManager.loadAndRenderProjectGeometries(geometryCollection);
                setTimeout(() => {
                    console.log('🌳 Calling renderProjectTree...');
                    uiManager.renderProjectTree();
                }, 100);
                showNotification(`Project "${project.title}" loaded successfully.`, 'info');
            } catch (error) {
                console.error('Failed to load project geometries.', error);

                showNotification('Failed to load project geometries.', 'error');
            } finally {
                // This is the key change:
                // Instead of just hiding the loading screen,
                // we tell the screen manager to show the main app.
                // showApp() will automatically hide the loading screen.
                showApp(true);
            }
        } else {
            console.error(`❌ Project with ID ${projectId} not found`);
            showNotification('Failed to load selected project.', 'error');
        }
    }

    closeActiveProject() {
        if (!this.activeProject) return;
        
        console.log(`🚪 Closing project: ${this.activeProject.title}`);
        const closedProjectTitle = this.activeProject.title;
        this.activeProject = null;
        updateProjectBar(null);
        showNotification(`Project "${closedProjectTitle}" closed.`, 'info');
    }

    showLoginFlow() {
        console.log('🔐 Showing login flow');
        
        // Hide loading screen and show login
        showLoading(false);
        showLoginScreen(true);
    
        const loginForm = document.getElementById('login-form');
        if (loginForm) {
            // Remove existing listeners by cloning
            const newForm = loginForm.cloneNode(true);
            loginForm.parentNode.replaceChild(newForm, loginForm);
    
            newForm.addEventListener('submit', async (e) => {
                e.preventDefault();
    
                const username = document.getElementById('username').value.trim();
                const password = document.getElementById('password').value;
    
                if (!username || !password) {
                    this.showLoginError('Please enter both username and password');
                    return;
                }
    
                // Get button elements
                const btn = document.getElementById('login-btn');
                const btnText = document.getElementById('login-btn-text');
                const spinner = document.getElementById('login-spinner');
                const errorEl = document.getElementById('login-error');
    
                // Set loading state
                if (btn) btn.disabled = true;
                if (btnText) btnText.classList.add('hidden');
                if (spinner) spinner.classList.remove('hidden');
                if (errorEl) errorEl.classList.add('hidden');
    
                try {
                    const result = await authManager.login(username, password);
    
                    if (result.success) {
                        console.log('✅ Login successful, initializing app...');
                        await this.initializeAuthenticatedApp();
                    } else {
                        this.showLoginError(result.error);
                        // Reset button state on failure
                        this.resetLoginButton(btn, btnText, spinner);
                    }
                } catch (error) {
                    console.error('❌ Login failed:', error);
                    this.showLoginError('An unexpected error occurred. Please try again.');
                    this.resetLoginButton(btn, btnText, spinner);
                }
            });
        }
    
        // Focus on username field
        setTimeout(() => {
            const usernameField = document.getElementById('username');
            if (usernameField) {
                usernameField.focus();
            }
        }, 100);
    }

    resetLoginButton(btn, btnText, spinner) {
        if (btn) btn.disabled = false;
        if (btnText) btnText.classList.remove('hidden');
        if (spinner) spinner.classList.add('hidden');
    }
    
    showLoginError(message) {
        const loginError = document.getElementById('login-error');
        if (loginError) {
            loginError.textContent = message;
            loginError.classList.remove('hidden');
            loginError.style.animation = 'shake 0.5s ease-in-out';
            setTimeout(() => { 
                if (loginError) loginError.style.animation = ''; 
            }, 500);
        }
    }

    setupLogoutHandler() {
        const logoutBtn = document.getElementById('logout-btn');
        if (logoutBtn) {
            // Ensure no duplicate listeners
            const newLogoutBtn = logoutBtn.cloneNode(true);
            logoutBtn.parentNode.replaceChild(newLogoutBtn, logoutBtn);

            newLogoutBtn.addEventListener('click', async () => {
                console.log('🔓 Logging out...');
                await authManager.logout();
                window.location.reload();
            });
        }
    }

    setupConflictRefresh() {
        this.conflictRefreshInterval = setInterval(async () => {
            if (window.mapManager?.refreshConflicts) {
                await window.mapManager.refreshConflicts();
            }
        }, 30000);
    }

    // --- NEW FUNCTION TO SET UP THE EVENT LISTENER ---
    setupProjectSubmissionHandler() {
        const submitBtn = document.getElementById('submit-project-btn');
        if (submitBtn) {
            submitBtn.addEventListener('click', (e) => {
                e.preventDefault();
                this.submitActiveProject();
            });
        }
    }

    // --- NEW FUNCTION FOR THE SUBMISSION LOGIC ---
    async submitActiveProject() {
        // 1. Check if there is an active project
        if (!this.activeProject) {
            showNotification('No active project to submit.', 'warning');
            return;
        }
    
        // 2. Confirm with the user
        if (!confirm(`Are you sure you want to submit the project "${this.activeProject.title}" for processing?`)) {
            return;
        }
    
        try {
            showLoading(true); // Show a loading spinner
    
            // 3. Get all drone zone geometries from the map
            const geometries = window.mapManager.droneZones;
            if (geometries.length === 0) {
                showNotification('This project has no drone zones to process.', 'warning');
                showLoading(false);
                return;
            }
    
            // 4. Call the API to submit the data and wait for the report
            const reportData = await apiClient.submitProject(this.activeProject.id, geometries);
    
            // 5. If successful, show the report modal
            if (reportData) {
                uiManager.showReportModal(reportData, this.activeProject);
            }
    
        } catch (error) {
            showNotification(`Error submitting project: ${error.message}`, 'error');
        } finally {
            showLoading(false); // Hide the loading spinner
        }
    }

    async loadProcedureControls() {
        try {
            console.log('📋 Loading procedure controls...');
            
            // Initialize procedure UI events
            if (uiManager.initializeProcedureEvents) {
                uiManager.initializeProcedureEvents();
            }
            
            // Wait for map to be initialized
            let attempts = 0;
            const maxAttempts = 20;
            
            while (attempts < maxAttempts) {
                if (mapManager.map && mapManager.procedures) {
                    console.log('✅ Map and procedures ready, rendering controls');
                    
                    // Get procedure statistics
                    const stats = mapManager.getProcedureStats ? mapManager.getProcedureStats() : null;
                    if (stats) {
                        console.log('📊 Procedure statistics:', stats);
                    }
                    
                    // Render procedure controls
                    renderProcedureControls(mapManager.procedures);
                    return;
                }
                
                console.log(`⏳ Waiting for procedures... attempt ${attempts + 1}/${maxAttempts}`);
                await new Promise(resolve => setTimeout(resolve, 500));
                attempts++;
            }
            
            console.log('⚠️ Procedures not loaded after waiting, using empty list');
            renderProcedureControls([]);
            
        } catch (error) {
            console.error('❌ Failed to load procedure controls:', error);
            renderProcedureControls([]);
        }
    }

    async loadAirportControls() {
        try {
            console.log('🛩️ Loading airport controls...');
            
            let attempts = 0;
            const maxAttempts = 20;
            
            while (attempts < maxAttempts) {
                if (mapManager.map && mapManager.airports) {
                    console.log('✅ Map and airports ready, rendering controls');
                    
                    // Get airport statistics
                    const stats = mapManager.getAirportStats ? mapManager.getAirportStats() : null;
                    if (stats) {
                        console.log('📊 Airport statistics:', stats);
                    }
                    
                    // Render airport controls
                    renderAirportControls(mapManager.airports);
                    return;
                }
                
                console.log(`⏳ Waiting for airports... attempt ${attempts + 1}/${maxAttempts}`);
                await new Promise(resolve => setTimeout(resolve, 500));
                attempts++;
            }
            
            console.log('⚠️ Airports not loaded after waiting, using empty list');
            renderAirportControls([]);
            
        } catch (error) {
            console.error('❌ Failed to load airport controls:', error);
            renderAirportControls([]);
        }
    }

    showError(message) {
        console.error('💥 Showing error screen:', message);
        showLoading(false);
        
        // Remove any existing error screens
        const existingError = document.querySelector('.error-screen');
        if (existingError) {
            existingError.remove();
        }
        
        const errorScreen = document.createElement('div');
        errorScreen.className = 'screen error-screen';
        errorScreen.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: linear-gradient(135deg, #dc2626, #b91c1c);
            color: white;
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 9999;
        `;
        errorScreen.innerHTML = `
            <div class="error-container" style="
                text-align: center;
                padding: 40px;
                background: rgba(255, 255, 255, 0.1);
                border-radius: 12px;
                backdrop-filter: blur(10px);
                max-width: 500px;
            ">
                <h1 style="font-size: 2rem; margin-bottom: 20px;">⚠️ Application Error</h1>
                <p style="margin-bottom: 30px; font-size: 1.1rem;">${message}</p>
                <button onclick="window.location.reload()" style="
                    padding: 15px 30px;
                    font-size: 1.1rem;
                    background: white;
                    color: #dc2626;
                    border: none;
                    border-radius: 6px;
                    cursor: pointer;
                    font-weight: 600;
                ">🔄 Reload Application</button>
            </div>
        `;
        document.body.appendChild(errorScreen);
    }

    // First, add the event listener setup inside the initializeAuthenticatedApp function
setupProjectSubmissionHandler() {
    const submitBtn = document.getElementById('submit-project-btn');
    if (submitBtn) {
        submitBtn.addEventListener('click', (e) => {
            e.preventDefault();
            this.submitActiveProject();
        });
    }
}

// Then, add the main submission logic as a new method in the class
async submitActiveProject() {
    // 1. Check if there is an active project
    if (!this.activeProject) {
        showNotification('No active project to submit.', 'warning');
        return;
    }

    // 2. Confirm with the user
    if (!confirm(`Are you sure you want to submit the project "${this.activeProject.title}" for processing?`)) {
        return;
    }

    try {
        showLoading(true); // Show a loading spinner

        // 3. Get all drone zone geometries from the map
        const geometries = window.mapManager.droneZones;
        if (geometries.length === 0) {
            showNotification('This project has no drone zones to process.', 'warning');
            return;
        }

        // 4. Call the API to submit the data and wait for the report
        const reportData = await apiClient.submitProject(this.activeProject.id, geometries);

        // 5. If successful, show the report modal
        if (reportData) {
            uiManager.showReportModal(reportData, this.activeProject);
        }

    } catch (error) {
        showNotification(`Error submitting project: ${error.message}`, 'error');
    } finally {
        showLoading(false); // Hide the loading spinner
    }
}
}

// Initialize application when DOM is ready
document.addEventListener('DOMContentLoaded', async () => {
    console.log('🌟 DOM Content Loaded, starting app initialization...');
    
    try {
        const app = new AeronauticalApp();
        window.aeronauticalApp = app; // Make accessible globally
        await app.init();
    } catch (error) {
        console.error('💥 Critical error during app initialization:', error);
        
        // Show emergency error screen
        document.body.innerHTML = `
            <div style="
                position: fixed;
                top: 0;
                left: 0;
                width: 100%;
                height: 100%;
                background: #dc2626;
                color: white;
                display: flex;
                align-items: center;
                justify-content: center;
                font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
                z-index: 10000;
            ">
                <div style="text-align: center; padding: 40px;">
                    <h1>💥 Critical Error</h1>
                    <p style="margin: 20px 0;">The application failed to start.</p>
                    <p style="margin: 20px 0; font-size: 0.9rem; opacity: 0.8;">Check the browser console for details.</p>
                    <button onclick="window.location.reload()" style="
                        padding: 15px 30px;
                        font-size: 1.1rem;
                        background: white;
                        color: #dc2626;
                        border: none;
                        border-radius: 6px;
                        cursor: pointer;
                        font-weight: 600;
                    ">🔄 Reload</button>
                </div>
            </div>
        `;
    }
});

// Global error handlers
window.addEventListener('error', (event) => {
    console.error('🚨 Global error:', event.error);
    console.error('Stack:', event.error?.stack);
});

window.addEventListener('unhandledrejection', (event) => {
    console.error('🚨 Unhandled promise rejection:', event.reason);
    console.error('Promise:', event.promise);
});