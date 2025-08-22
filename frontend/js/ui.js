// UI components and interaction handlers
import authManager from './auth.js';
import mapManager from './map.js';
import apiClient from './api.js';
import ImageEditor from './image-editor.js';

class UIManager {
    constructor() {
        this.currentFormCallback = null;
        this.procedures = [];
        this.notifications = [];
        this.activeProject = null;
        this.projects = [];
        this.imageEditor = null;
    }

    async init() {
        console.log('🔄 UIManager: Starting initialization...');
        
        try {
            console.log('🔄 UIManager: Loading modals...');
            await this.loadModals();
            console.log('✅ UIManager: Modals loaded');
            
            console.log('🔄 UIManager: Setting up modal handlers...');
            this.setupModalHandlers();
            console.log('✅ UIManager: Modal handlers set up');
            
            console.log('🔄 UIManager: Setting up form handlers...');
            this.setupFormHandlers();
            console.log('✅ UIManager: Form handlers set up');
            
            console.log('🔄 UIManager: Setting up screen transitions...');
            this.setupScreenTransitions();
            console.log('✅ UIManager: Screen transitions set up');
            
            console.log('🔄 UIManager: Setting up project modal handlers...');
            this.setupProjectModalHandlers();
            console.log('✅ UIManager: Project modal handlers set up');
            
            console.log('🔄 UIManager: Setting up operations dropdown...');
            this.setupOperationsDropdown();
            console.log('✅ UIManager: Operations dropdown set up');
            
            console.log('🔄 UIManager: Setting up import buttons...');
            this.setupImportButtons();
            console.log('✅ UIManager: Import buttons set up');

            console.log('🔄 UIManager: Setting up Report UI...');
            this.setupReportModalHandlers();
            console.log('✅ UIManager: Report UI set up');

            
            console.log('✅ UIManager: Initialization complete');
            
        } catch (error) {
            console.error('❌ UIManager: Initialization failed:', error);
            console.error('❌ UIManager: Error stack:', error.stack);
            throw error;
        }
    }
    
    // Also add this alternative loadModals method that doesn't depend on external files:
    async loadModalsAlternative() {
        console.log('🔄 UIManager: Loading modals (alternative method)...');
        
        // Instead of loading from external file, create the modal directly
        const imageEditorModal = `
            <div id="image-editor-modal" class="modal hidden" role="dialog" aria-labelledby="image-editor-title" aria-hidden="true">
                <div class="modal-content wide">
                    <div class="modal-header">
                        <h2 id="image-editor-title">Image-based Geometry Creator</h2>
                        <button id="close-image-editor-btn" class="close-button" aria-label="Close dialog">&times;</button>
                    </div>
                    <div class="modal-body image-editor-body">
                        <div class="editor-main-area">
                            <canvas id="image-canvas" class="image-canvas"></canvas>
                        </div>
                        <div class="editor-controls-area">
                            <h4>1. Adjust Image</h4>
                            <div class="control-group">
                                <label for="rotate-slider">Rotate (<span>0</span>&deg;)</label>
                                <input type="range" id="rotate-slider" min="-180" max="180" value="0" class="slider">
                            </div>
                            <h4>2. Select Area & Run OCR</h4>
                            <p class="instructions">Click and drag on the image to select a region for text recognition.</p>
                            <button id="run-ocr-btn" class="button primary full-width" disabled>Run OCR on Selection</button>
                            <div id="ocr-status" class="ocr-status-text"></div>
                            <h4>3. Review & Create Geometry</h4>
                            <div class="control-group">
                                <label for="ocr-results">Recognized Text:</label>
                                <textarea id="ocr-results" rows="6" placeholder="OCR results will appear here..."></textarea>
                            </div>
                            <div class="control-group">
                                <label for="geometry-type-selector">Geometry Type:</label>
                                <select id="geometry-type-selector">
                                    <option value="polygon">Polygon</option>
                                    <option value="path">Path with Width</option>
                                    <option value="points">Points with Radius</option>
                                </select>
                            </div>
                            <div id="geometry-options"></div>
                            <div class="form-actions">
                                <button id="create-geometry-btn" class="button primary" disabled>Create Geometry</button>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        `;
        
        const modalsContainer = document.getElementById('modals-container');
        if (modalsContainer) {
            modalsContainer.insertAdjacentHTML('beforeend', imageEditorModal);
            console.log('✅ UIManager: Image editor modal added directly');
        } else {
            console.error('❌ UIManager: modals-container not found');
            throw new Error('modals-container element not found');
        }
    }

    createProcedureFilters(procedures) {
        const filterDiv = document.createElement('div');
        filterDiv.className = 'procedure-filters';
        
        // Get unique procedure types
        const types = [...new Set(procedures.map(p => p.type))].sort();
        
        // Data source indicator
        const dataSource = window.apiClient ? window.apiClient.getDataSource() : 'unknown';
        const sourceColor = dataSource === 'database' ? '#10b981' : '#f59e0b';
        const sourceIcon = dataSource === 'database' ? '🗄️' : '🧪';
        
        filterDiv.innerHTML = `
            <div class="filter-header">
                <div class="data-source-indicator" style="
                    background: ${sourceColor}20; 
                    color: ${sourceColor}; 
                    padding: 4px 8px; 
                    border-radius: 6px; 
                    font-size: 11px; 
                    font-weight: 600;
                    display: inline-flex;
                    align-items: center;
                    gap: 4px;
                    margin-bottom: 10px;
                ">
                    ${sourceIcon} ${dataSource.toUpperCase()} DATA
                </div>
            </div>
            
            <div class="filter-controls" style="margin-bottom: 15px;">
                <div class="filter-group">
                    <label for="type-filter" style="font-size: 12px; font-weight: 500; color: #374151;">Filter by Type:</label>
                    <select id="type-filter" style="
                        width: 100%; 
                        padding: 6px; 
                        border: 1px solid #d1d5db; 
                        border-radius: 4px; 
                        font-size: 12px;
                        margin-top: 4px;
                    ">
                        <option value="all">All Types (${procedures.length})</option>
                        ${types.map(type => {
                            const count = procedures.filter(p => p.type === type).length;
                            return `<option value="${type}">${type} (${count})</option>`;
                        }).join('')}
                    </select>
                </div>
            </div>
            
            <div class="filter-actions" style="margin-bottom: 15px;">
                <button id="show-all-procedures" style="
                    background: #10b981; color: white; border: none; 
                    padding: 6px 12px; border-radius: 4px; 
                    font-size: 11px; cursor: pointer; margin-right: 5px;
                ">Show All</button>
                <button id="hide-all-procedures" style="
                    background: #6b7280; color: white; border: none; 
                    padding: 6px 12px; border-radius: 4px; 
                    font-size: 11px; cursor: pointer; margin-right: 5px;
                ">Hide All</button>
                <button id="refresh-procedures" style="
                    background: #3b82f6; color: white; border: none; 
                    padding: 6px 12px; border-radius: 4px; 
                    font-size: 11px; cursor: pointer;
                ">🔄 Refresh</button>
            </div>
        `;
        
        // Add event listeners
        const typeFilter = filterDiv.querySelector('#type-filter');
        typeFilter.addEventListener('change', (e) => {
            if (window.mapManager) {
                window.mapManager.filterProceduresByType(e.target.value);
            }
        });
        
        const showAllBtn = filterDiv.querySelector('#show-all-procedures');
        showAllBtn.addEventListener('click', () => {
            procedures.forEach(p => p.isVisible = true);
            if (window.mapManager) {
                window.mapManager.renderProcedures();
            }
            this.renderProcedureControls(procedures);
        });
        
        const hideAllBtn = filterDiv.querySelector('#hide-all-procedures');
        hideAllBtn.addEventListener('click', () => {
            procedures.forEach(p => p.isVisible = false);
            if (window.mapManager) {
                window.mapManager.renderProcedures();
            }
            this.renderProcedureControls(procedures);
        });
        
        const refreshBtn = filterDiv.querySelector('#refresh-procedures');
        refreshBtn.addEventListener('click', async () => {
            if (window.mapManager && window.mapManager.refreshProcedures) {
                await window.mapManager.refreshProcedures();
            }
        });
        
        return filterDiv;
    }

    createProcedureStats(procedures) {
        const statsDiv = document.createElement('div');
        statsDiv.className = 'procedure-stats';
        
        const visibleCount = procedures.filter(p => p.isVisible !== false).length;
        const totalSegments = procedures.reduce((sum, p) => sum + (p.segmentCount || 0), 0);
        const totalProtections = procedures.reduce((sum, p) => sum + (p.protectionCount || 0), 0);
        
        // Count by type
        const typeStats = {};
        procedures.forEach(p => {
            if (!typeStats[p.type]) typeStats[p.type] = 0;
            typeStats[p.type]++;
        });
        
        statsDiv.innerHTML = `
            <div class="stats-grid" style="
                display: grid; 
                grid-template-columns: 1fr 1fr; 
                gap: 8px; 
                margin-bottom: 15px;
                font-size: 11px;
            ">
                <div class="stat-item" style="
                    background: #f8fafc; 
                    padding: 8px; 
                    border-radius: 6px; 
                    text-align: center;
                ">
                    <div style="font-weight: 600; color: #1e40af;">${visibleCount}/${procedures.length}</div>
                    <div style="color: #6b7280;">Visible</div>
                </div>
                <div class="stat-item" style="
                    background: #f8fafc; 
                    padding: 8px; 
                    border-radius: 6px; 
                    text-align: center;
                ">
                    <div style="font-weight: 600; color: #1e40af;">${totalSegments}</div>
                    <div style="color: #6b7280;">Segments</div>
                </div>
                <div class="stat-item" style="
                    background: #f8fafc; 
                    padding: 8px; 
                    border-radius: 6px; 
                    text-align: center;
                ">
                    <div style="font-weight: 600; color: #1e40af;">${totalProtections}</div>
                    <div style="color: #6b7280;">Protections</div>
                </div>
                <div class="stat-item" style="
                    background: #f8fafc; 
                    padding: 8px; 
                    border-radius: 6px; 
                    text-align: center;
                ">
                    <div style="font-weight: 600; color: #1e40af;">${Object.keys(typeStats).length}</div>
                    <div style="color: #6b7280;">Types</div>
                </div>
            </div>
            
            <div class="type-breakdown" style="
                background: #f8fafc; 
                padding: 8px; 
                border-radius: 6px; 
                margin-bottom: 15px;
            ">
                <div style="font-weight: 600; font-size: 11px; color: #374151; margin-bottom: 6px;">By Type:</div>
                ${Object.entries(typeStats).map(([type, count]) => `
                    <div style="
                        display: flex; 
                        justify-content: space-between; 
                        font-size: 10px; 
                        color: #6b7280; 
                        margin-bottom: 2px;
                    ">
                        <span>${type}</span>
                        <span>${count}</span>
                    </div>
                `).join('')}
            </div>
        `;
        
        return statsDiv;
    }


    createProcedureList(procedures) {
        const listDiv = document.createElement('div');
        listDiv.className = 'procedure-list';
        
        if (procedures.length === 0) {
            listDiv.innerHTML = `
                <div class="no-procedures" style="
                    text-align: center; 
                    padding: 20px; 
                    color: #6b7280; 
                    font-size: 12px;
                ">
                    No procedures available
                </div>
            `;
            return listDiv;
        }
        
        // Sort procedures by type, then by code
        const sortedProcedures = [...procedures].sort((a, b) => {
            if (a.type !== b.type) return a.type.localeCompare(b.type);
            return a.procedure_code.localeCompare(b.procedure_code);
        });
        
        listDiv.innerHTML = `
            <div class="list-header" style="
                font-weight: 600; 
                font-size: 11px; 
                color: #374151; 
                margin-bottom: 8px; 
                padding-bottom: 6px; 
                border-bottom: 1px solid #e5e7eb;
            ">
                Flight Procedures (${procedures.length})
            </div>
            <div class="list-container" style="max-height: 200px; overflow-y: auto;">
                ${sortedProcedures.map(procedure => this.createProcedureItem(procedure)).join('')}
            </div>
        `;
        
        // Add event listeners to procedure items
        listDiv.querySelectorAll('.procedure-item').forEach(item => {
            const procedureId = item.dataset.procedureId;
            const procedure = procedures.find(p => p.id == procedureId);
            
            if (procedure) {
                // Click to focus
                item.addEventListener('click', () => {
                    if (window.mapManager) {
                        window.mapManager.focusOnProcedure(procedureId);
                    }
                });
                
                // Toggle visibility
                const toggleBtn = item.querySelector('.visibility-toggle');
                if (toggleBtn) {
                    toggleBtn.addEventListener('click', (e) => {
                        e.stopPropagation();
                        const newVisibility = !procedure.isVisible;
                        if (window.mapManager) {
                            window.mapManager.toggleProcedureVisibility(procedureId, newVisibility);
                        }
                    });
                }
            }
        });
        
        return listDiv;
    }
    
    createProcedureItem(procedure) {
        const isVisible = procedure.isVisible !== false;
        const visibilityIcon = isVisible ? '👁️' : '🙈';
        const itemOpacity = isVisible ? '1' : '0.5';
        
        // Type-specific styling
        const typeColors = {
            'SID': '#10b981',
            'DEPARTURE': '#10b981',
            'STAR': '#f59e0b', 
            'ARRIVAL': '#f59e0b',
            'APPROACH': '#8b5cf6'
        };
        
        const typeColor = typeColors[procedure.type] || '#6b7280';
        
        return `
            <div class="procedure-item" 
                 data-procedure-id="${procedure.id}" 
                 style="
                    display: flex; 
                    align-items: center; 
                    justify-content: space-between; 
                    padding: 8px; 
                    margin-bottom: 4px; 
                    background: white; 
                    border: 1px solid #e5e7eb; 
                    border-radius: 6px; 
                    cursor: pointer; 
                    opacity: ${itemOpacity};
                    transition: all 0.2s ease;
                 "
                 onmouseover="this.style.backgroundColor='#f8fafc'"
                 onmouseout="this.style.backgroundColor='white'">
                
                <div class="procedure-info" style="flex: 1; min-width: 0;">
                    <div class="procedure-header" style="
                        display: flex; 
                        align-items: center; 
                        gap: 6px; 
                        margin-bottom: 2px;
                    ">
                        <span class="procedure-code" style="
                            font-weight: 600; 
                            font-size: 11px; 
                            color: #1e40af;
                        ">${procedure.procedure_code}</span>
                        
                        <span class="procedure-type" style="
                            background: ${typeColor}20; 
                            color: ${typeColor}; 
                            padding: 1px 4px; 
                            border-radius: 3px; 
                            font-size: 9px; 
                            font-weight: 600;
                        ">${procedure.type}</span>
                    </div>
                    
                    <div class="procedure-details" style="
                        font-size: 10px; 
                        color: #6b7280; 
                        overflow: hidden; 
                        text-overflow: ellipsis; 
                        white-space: nowrap;
                    ">
                        ${procedure.airport_icao}${procedure.runway ? ` RWY ${procedure.runway}` : ''}
                        ${procedure.segmentCount ? ` • ${procedure.segmentCount} seg` : ''}
                        ${procedure.protectionCount ? ` • ${procedure.protectionCount} prot` : ''}
                    </div>
                </div>
                
                <div class="procedure-actions" style="
                    display: flex; 
                    align-items: center; 
                    gap: 4px;
                ">
                    <button class="visibility-toggle" 
                            title="${isVisible ? 'Hide' : 'Show'} procedure"
                            style="
                                background: none; 
                                border: none; 
                                font-size: 12px; 
                                cursor: pointer; 
                                padding: 2px;
                            ">${visibilityIcon}</button>
                </div>
            </div>
        `;
    }
    
    
    async loadModals() {
        try {
            // Fetching the external HTML for the modal is an async operation
            const response = await fetch('image-editor.html');
            if (!response.ok) {
                throw new Error(`Failed to load image-editor.html: ${response.statusText}`);
            }
            const html = await response.text();
            document.getElementById('modals-container').insertAdjacentHTML('beforeend', html);
        } catch (error) {
            console.error('Failed to load image editor modal:', error);
            // Notify the user that a feature might be unavailable
            showNotification('Could not load image editor component.', 'error');
        }
    }

    setupImportButtons() {
        const importImageBtn = document.getElementById('import-image-btn');
        if (importImageBtn) {
            importImageBtn.addEventListener('click', () => {
                const fileInput = document.createElement('input');
                fileInput.type = 'file';
                fileInput.accept = 'image/*';
                fileInput.onchange = (e) => {
                    const file = e.target.files[0];
                    if (file) {
                        this.openImageEditor(file);
                    }
                };
                fileInput.click();
            });
        }
    }

    openImageEditor(file) {
        if (!this.imageEditor) {
            // Lazy initialization of the ImageEditor
            this.imageEditor = new ImageEditor('image-editor-modal', (geometryData) => {
                console.log('Geometry created from image:', geometryData);
                // Placeholder for future functionality to add the geometry to the map
                showNotification('Geometry created from image successfully!', 'success');
            });
        }
        this.imageEditor.show(file);
    }

    setupScreenTransitions() {
        if (window.screenManager) {
            this.screenManager = window.screenManager;
        }
    }

    setupModalHandlers() {
        const modal = document.getElementById('drone-form-modal');
        const closeBtn = document.getElementById('close-modal');
        const cancelBtn = document.getElementById('cancel-drone-form');

        [closeBtn, cancelBtn].forEach(btn => {
            if (btn) btn.addEventListener('click', (e) => {
                e.preventDefault();
                this.hideDroneZoneForm();
            });
        });

        if (modal) modal.addEventListener('click', (e) => {
            if (e.target === modal) this.hideDroneZoneForm();
        });

        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape' && modal && !modal.classList.contains('hidden')) {
                this.hideDroneZoneForm();
            }
        });
    }

    setupProjectModalHandlers() {
        const openBtn = document.getElementById('open-project-btn');
        const closeBtn = document.getElementById('close-project-modal-btn');
        const modal = document.getElementById('project-list-modal');

        if (openBtn) openBtn.addEventListener('click', () => this.showProjectListModal());
        if (closeBtn) closeBtn.addEventListener('click', () => this.hideProjectListModal());
        if (modal) modal.addEventListener('click', (e) => {
            if (e.target === modal) this.hideProjectListModal();
        });

        const closeProjectBtn = document.getElementById('close-project-btn');
        if (closeProjectBtn) closeProjectBtn.addEventListener('click', () => {
            if (window.aeronauticalApp) window.aeronauticalApp.closeActiveProject();
        });
    }
    
    setupOperationsDropdown() {
        const dropdownBtn = document.getElementById('operations-dropdown-btn');
        const dropdownMenu = document.getElementById('operations-dropdown-menu');

        if (dropdownBtn && dropdownMenu) {
            dropdownBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                dropdownMenu.classList.toggle('hidden');
            });

            document.addEventListener('click', (e) => {
                if (!dropdownMenu.classList.contains('hidden') && !dropdownBtn.contains(e.target)) {
                    dropdownMenu.classList.add('hidden');
                }
            });
        }
    }

    showProjectListModal() {
        const modal = document.getElementById('project-list-modal');
        if (modal) {
            modal.classList.remove('hidden');
            modal.setAttribute('aria-hidden', 'false');
            modal.style.display = 'flex';
        }
    }

    hideProjectListModal() {
        const modal = document.getElementById('project-list-modal');
        if (modal) {
            modal.classList.add('hidden');
            modal.setAttribute('aria-hidden', 'true');
            modal.style.display = 'none';
        }
    }
    // Method to update procedure controls when data changes
    updateProcedureControls(procedures) {
        console.log('🔄 Updating procedure controls...');
        this.renderProcedureControls(procedures);
    }

    // Method to handle procedure selection events
handleProcedureSelection(procedure) {
    console.log('🎯 Procedure selected in UI:', procedure.procedure_code);
    
    // Update UI to show selected state
    const procedureItems = document.querySelectorAll('.procedure-item');
    procedureItems.forEach(item => {
        if (item.dataset.procedureId == procedure.id) {
            item.style.backgroundColor = '#dbeafe';
            item.style.borderColor = '#3b82f6';
        } else {
            item.style.backgroundColor = 'white';
            item.style.borderColor = '#e5e7eb';
        }
    });
    
    // Show notification
    if (window.showNotification) {
        window.showNotification(
            `Selected: ${procedure.procedure_code} - ${procedure.name}`, 
            'info'
        );
    }
}

// Initialize procedure event listeners
initializeProcedureEvents() {
    // Listen for procedure selection events from map
    document.addEventListener('procedureSelected', (event) => {
        this.handleProcedureSelection(event.detail.procedure);
    });
    
    console.log('✅ Procedure UI events initialized');
}


    updateProjectBar(project) {
        this.activeProject = project;
        const opener = document.getElementById('project-opener');
        const display = document.getElementById('project-display');
        const titleEl = document.getElementById('project-title-text');
        const dropdownMenu = document.getElementById('operations-dropdown-menu');
        const drawingToolsPanel = document.getElementById('drawing-tools-panel');
        const importToolsPanel = document.getElementById('import-tools-panel');

        if (project) {
            opener.classList.add('hidden');
            display.classList.remove('hidden');
            if (drawingToolsPanel) drawingToolsPanel.classList.remove('hidden');
            if (importToolsPanel) importToolsPanel.classList.remove('hidden');
            titleEl.textContent = `${project.project_code} - ${project.title}`;
        } else {
            opener.classList.remove('hidden');
            display.classList.add('hidden');
            if (drawingToolsPanel) drawingToolsPanel.classList.add('hidden');
            if (importToolsPanel) importToolsPanel.classList.add('hidden');
            titleEl.textContent = '';
            if (dropdownMenu) dropdownMenu.classList.add('hidden');
        }
    }

    renderProjectList(projects) {
        this.projects = projects;
        const container = document.getElementById('project-list-container');
        if (!container) return;

        container.innerHTML = '';

        if (!projects || projects.length === 0) {
            container.innerHTML = '<div class="loading-projects">No projects available.</div>';
            return;
        }

        projects.forEach(project => {
            const item = document.createElement('div');
            item.className = 'project-list-item';
            item.dataset.projectId = project.id;
            item.innerHTML = `
                <div class="project-item-title">${project.title}</div>
                <div class="project-item-details">
                    <span>${project.project_code}</span>
                    <span>${project.status}</span>
                </div>
            `;

            item.addEventListener('click', () => {
                if (window.aeronauticalApp) {
                    window.aeronauticalApp.setActiveProject(project.id);
                }
            });

            container.appendChild(item);
        });
    }

    setupFormHandlers() {
        const form = document.getElementById('drone-zone-form');
        
        if (form) {
            form.addEventListener('submit', (e) => {
                e.preventDefault();
                this.handleFormSubmit();
            });
            this.setupFormValidation();
        }
        this.setDefaultFormTimes();
    }

    setupFormValidation() {}
    showFieldError(fieldId, message) {}
    clearFieldError(fieldId) {}
    clearAllFieldErrors() {}

    setDefaultFormTimes() {
        const startTimeField = document.getElementById('start-time');
        const endTimeField = document.getElementById('end-time');
        
        if (startTimeField && endTimeField) {
            const now = new Date();
            const startTime = new Date(now.getTime() + 60 * 60 * 1000);
            const endTime = new Date(startTime.getTime() + 2 * 60 * 60 * 1000);

            startTimeField.value = this.formatDateTimeLocal(startTime);
            endTimeField.value = this.formatDateTimeLocal(endTime);
        }
    }

    formatDateTimeLocal(date) {
        const pad = (num) => num.toString().padStart(2, '0');
        return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}T${pad(date.getHours())}:${pad(date.getMinutes())}`;
    }

    showDroneZoneForm(geometry, callback) {
        this.currentFormCallback = callback;
        const modal = document.getElementById('drone-form-modal');
        if (!modal) return;
        
        this.setDefaultFormTimes();
        this.clearAllFieldErrors();
        
        const timestamp = new Date().toISOString().slice(0, 16).replace(/[-:]/g, '').replace('T', '-');
        const operationIdField = document.getElementById('operation-id');
        if (operationIdField) {
            operationIdField.value = `DRONE-${timestamp}`;
        }
        
        modal.classList.remove('hidden');
        modal.style.display = 'flex';
        
        setTimeout(() => {
            const firstInput = document.getElementById('operation-id');
            if (firstInput) firstInput.focus();
        }, 100);
    }

    hideDroneZoneForm() {
        const modal = document.getElementById('drone-form-modal');
        if (modal) {
            modal.classList.add('hidden');
            modal.style.display = 'none';
        }
        
        const form = document.getElementById('drone-zone-form');
        if (form) form.reset();
        
        this.clearAllFieldErrors();
        this.currentFormCallback = null;
        
        if (window.mapManager && window.mapManager.currentDrawnLayer) {
            window.mapManager.drawnItems.removeLayer(window.mapManager.currentDrawnLayer);
            window.mapManager.currentDrawnLayer = null;
        }
    }

    handleFormSubmit() {
        console.log('🎛️ handleFormSubmit');


            // 1. Collect form data into an object
            const form = document.getElementById('drone-zone-form');
            const formData = new FormData(form);
            const data = {
                operationId: formData.get('operationId'),
                altitudeMin: formData.get('altitudeMin'),
                altitudeMax: formData.get('altitudeMax'),
                startTime: formData.get('startTime'),
                endTime: formData.get('endTime'),
                status: formData.get('status')
            };

            // 2. Execute the stored callback with the data, if it exists
            if (this.currentFormCallback) {
                this.currentFormCallback(data);
            }

            // 3. Hide the form
            this.hideDroneZoneForm();

    }
    validateForm(formData) { return true; }
    renderProcedureControls(procedures) {
        console.log('🎛️ Rendering procedure controls for', procedures.length, 'procedures');
        
        const container = document.getElementById('procedure-controls');
        if (!container) {
            console.warn('⚠️ Procedure controls container not found');
            return;
        }
        
        // Clear existing controls
        container.innerHTML = '';
        
        if (!procedures || procedures.length === 0) {
            container.innerHTML = `
                <div class="no-procedures">
                    <p>📡 Loading procedures from database...</p>
                </div>
            `;
            return;
        }
        
        // Create filter controls
        const filterControls = this.createProcedureFilters(procedures);
        container.appendChild(filterControls);
        
        // Create statistics panel
        const statsPanel = this.createProcedureStats(procedures);
        container.appendChild(statsPanel);
        
        // Create procedure list
        const procedureList = this.createProcedureList(procedures);
        container.appendChild(procedureList);
        
        console.log('✅ Procedure controls rendered');
    }
    updateConflictPanel(conflicts, onConflictClick) {}

    updateUserInfo(user) {
        const userInfoElement = document.getElementById('user-info');
        if (userInfoElement && user) {
            userInfoElement.textContent = user.fullName || user.username || 'User';
        }
    }

    showNotification(message, type = 'info', duration = 5000) {}
    removeNotification(notification) {}
    getNotificationIcon(type) {}
    ensureNotificationStyles() {}

    showLoading(show = true) {
        console.log(`🔄 showLoading called with: ${show}`);
        if (show) {
            if (window.screenManager) {
                window.screenManager.showScreen('loading-screen');
            } else {
                console.warn('⚠️ screenManager not available, using fallback');
                this.fallbackShowScreen('loading-screen');
            }
        } else {
            if (window.screenManager) {
                window.screenManager.hideScreen('loading-screen');
            }
        }
    }
    
    showLoginScreen(show = true) {
        console.log(`🔐 showLoginScreen called with: ${show}`);
        if (show) {
            if (window.screenManager) {
                window.screenManager.showScreen('login-screen');
            } else {
                console.warn('⚠️ screenManager not available, using fallback');
                this.fallbackShowScreen('login-screen');
            }
        } else {
            if (window.screenManager) {
                window.screenManager.hideScreen('login-screen');
            }
        }
    }
    
    showApp(show = true) {
        console.log(`🏠 showApp called with: ${show}`);
        if (show) {
            if (window.screenManager) {
                window.screenManager.showScreen('app');
            } else {
                console.warn('⚠️ screenManager not available, using fallback');
                this.fallbackShowScreen('app');
            }
        } else {
            if (window.screenManager) {
                window.screenManager.hideScreen('app');
            }
        }
    }

    fallbackShowScreen(screenId) {
        console.log(`🔧 Using fallback to show screen: ${screenId}`);
        
        // Hide all screens
        const screens = ['loading-screen', 'login-screen', 'app'];
        screens.forEach(id => {
            const element = document.getElementById(id);
            if (element) {
                element.classList.add('hidden');
                element.style.display = 'none';
            }
        });
        
        // Show target screen
        const targetScreen = document.getElementById(screenId);
        if (targetScreen) {
            targetScreen.classList.remove('hidden');
            targetScreen.style.display = 'flex';
            console.log(`✅ Fallback: Showing ${screenId}`);
        } else {
            console.error(`❌ Fallback: Screen not found: ${screenId}`);
        }
    }
    
    // Add this to setupScreenTransitions method
    setupScreenTransitions() {
        console.log('🖥️ Setting up screen transitions...');
        
        // Wait a bit for screenManager to be available
        setTimeout(() => {
            if (window.screenManager) {
                this.screenManager = window.screenManager;
                console.log('✅ Screen manager connected');
            } else {
                console.warn('⚠️ Screen manager not found, will use fallback methods');
            }
        }, 100);
    }

    destroy() {
        this.notifications.forEach(n => this.removeNotification(n));
        this.notifications = [];
    }


/**
 * Generates and displays the project submission report.
 * @param {object} reportData - The data returned from the backend.
 * @param {object} project - The active project object.
 */
showReportModal(reportData, project) {
    const reportBody = document.getElementById('report-body');
    const modal = document.getElementById('report-modal');

    if (!reportBody || !modal) return;

    // --- Helper function to create the report HTML ---
    const createReportHTML = (data, proj) => {
        const conflicts = data.conflicts || [];
        const hasConflicts = conflicts.length > 0;

        // Date formatting
        const formatDate = (dateString) => {
            if (!dateString) return 'N/A';
            return new Date(dateString).toLocaleString();
        };

        // Main report template
        return `
            <div class="report-container">
                <div class="report-header">
                    <h1>Preliminary Analysis Report</h1>
                    <p><strong>Project:</strong> ${proj.title}</p>
                </div>

                <div class="report-section">
                    <h2>Project Details</h2>
                    <table>
                        <tr>
                            <th>Project Code</th>
                            <td>${proj.project_code}</td>
                            <th>Status</th>
                            <td><span class="status-badge ${proj.status.toLowerCase()}">${proj.status}</span></td>
                        </tr>
                        <tr>
                            <th>Operation Type</th>
                            <td>${proj.operation_type || 'N/A'}</td>
                             <th>Priority</th>
                            <td>${proj.priority || 'N/A'}</td>
                        </tr>
                        <tr>
                            <th>Start Date</th>
                            <td>${formatDate(proj.start_date)}</td>
                             <th>End Date</th>
                            <td>${formatDate(proj.end_date)}</td>
                        </tr>
                        <tr>
                            <th>Min Altitude</th>
                            <td>${proj.altitude_min ?? 'N/A'} ft AGL</td>
                            <th>Max Altitude</th>
                            <td>${proj.altitude_max ?? 'N/A'} ft AGL</td>
                        </tr>
                    </table>
                </div>

                <div class="report-section">
                    <h2>Conflict Analysis</h2>
                    <div class="summary">
                        <div class="summary-item ${hasConflicts ? 'conflicts' : 'no-conflicts'}">
                            <span class="icon">${hasConflicts ? '⚠️' : '✅'}</span>
                            <div>
                                <strong>${conflicts.length} Conflicts Detected</strong>
                                <p>${hasConflicts ? 'Review the details below.' : 'No direct conflicts with flight procedures.'}</p>
                            </div>
                        </div>
                    </div>

                    ${hasConflicts ? `
                    <table class="conflict-table">
                        <thead>
                            <tr>
                                <th>Conflict ID</th>
                                <th>Affected Procedure</th>
                                <th>Conflict Type</th>
                                <th>Severity</th>
                                <th>Details</th>
                            </tr>
                        </thead>
                        <tbody>
                            ${conflicts.map(conflict => `
                                <tr>
                                    <td>${conflict.id}</td>
                                    <td>${conflict.procedure_code || 'N/A'}</td>
                                    <td><span class="badge ${conflict.type?.toLowerCase()}">${conflict.type || 'Unknown'}</span></td>
                                    <td><span class="badge severity-${conflict.severity?.toLowerCase()}">${conflict.severity || 'N/A'}</span></td>
                                    <td>${conflict.details || 'No additional details.'}</td>
                                </tr>
                            `).join('')}
                        </tbody>
                    </table>
                    ` : ''}
                </div>
            </div>
        `;
    };

    // --- Generate and inject the HTML ---
    reportBody.innerHTML = createReportHTML(reportData, project);

    // --- Show the modal ---
    modal.classList.remove('hidden');

    // --- Add event listeners for close and print buttons ---
    const closeModalBtn = document.getElementById('close-report-btn');
    const printBtn = document.getElementById('print-report-btn');

    const closeHandler = () => {
        modal.classList.add('hidden');
        closeModalBtn.removeEventListener('click', closeHandler);
    };

    const printHandler = () => {
        window.print();
    };
    
    closeModalBtn.addEventListener('click', closeHandler);
    printBtn.addEventListener('click', printHandler);
}

// This function handles closing the modal and printing
setupReportModalHandlers() {
    const closeBtn = document.getElementById('close-report-btn');
    const printBtn = document.getElementById('print-report-btn');

    if (closeBtn) {
        closeBtn.addEventListener('click', () => {
            const modal = document.getElementById('report-modal');
            if (modal) {
                modal.classList.add('hidden');
                modal.style.display = 'none';
            }
            // Add this line to show the main app again
            showApp(true); 
        });
    }

    if (printBtn) {
        printBtn.addEventListener('click', () => {
            window.print();
        });
    }
}




}

const uiManager = new UIManager();

// Export functions for use by other modules
export function showDroneZoneForm(geometry, callback) { uiManager.showDroneZoneForm(geometry, callback); }
export function updateConflictPanel(conflicts, onConflictClick) { uiManager.updateConflictPanel(conflicts, onConflictClick); }
export function renderProcedureControls(procedures) { uiManager.renderProcedureControls(procedures); }
export function updateUserInfo(user) { uiManager.updateUserInfo(user); }
export function showNotification(message, type = 'info', duration = 5000) { uiManager.showNotification(message, type, duration); }
export function showLoading(show = true) { uiManager.showLoading(show); }
export function showLoginScreen(show = true) { uiManager.showLoginScreen(show); }
export function showApp(show = true) { uiManager.showApp(show); }
export function updateProjectBar(project) { uiManager.updateProjectBar(project); }
export function renderProjectList(projects) { uiManager.renderProjectList(projects); }
export function hideProjectListModal() { uiManager.hideProjectListModal(); }

window.uiManager = uiManager;
export default uiManager;
