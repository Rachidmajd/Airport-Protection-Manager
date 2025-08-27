// Enhanced Image Editor with OCR Processing Modal
import { showNotification } from './ui.js';

// OCR Modal Manager Class
class OCRModalManager {
    constructor() {
        this.modal = null;
        this.content = null;
        this.init();
    }

    init() {
        // Create OCR processing modal if it doesn't exist
        if (!document.getElementById('ocr-processing-modal')) {
            this.createModal();
        }
        this.modal = document.getElementById('ocr-processing-modal');
        this.content = document.getElementById('ocr-content');
        
        // Add click outside to close
        this.modal.addEventListener('click', (e) => {
            if (e.target === this.modal) {
                this.hide();
            }
        });
    }

    createModal() {
        const modalHTML = `
            <div id="ocr-processing-modal" style="display: none;">
                <div class="ocr-modal-content">
                    <div id="ocr-content"></div>
                </div>
            </div>
        `;
        
        // Add to modals container or body
        const container = document.getElementById('modals-container') || document.body;
        container.insertAdjacentHTML('beforeend', modalHTML);
    }

    show() {
        this.modal.classList.remove('hidden'); 
        this.modal.style.display = 'flex';
        setTimeout(() => this.modal.classList.add('show'), 10);
        document.body.style.overflow = 'hidden'; // Prevent background scroll
    }

    hide() {
        this.modal.classList.remove('show');
        setTimeout(() => {
            this.modal.style.display = 'none';
            document.body.style.overflow = ''; // Restore scroll
        }, 300);
    }

    showProcessing(message = 'Processing image with OCR...') {
        this.content.innerHTML = `
            <div class="ocr-scanner"></div>
            <div class="ocr-status-text">${message}</div>
            <div class="ocr-progress-text">Analyzing text regions in the image...</div>
            <div class="ocr-text-processing">
                <span>Processing</span>
                <div class="ocr-processing-dots">
                    <div class="ocr-dot"></div>
                    <div class="ocr-dot"></div>
                    <div class="ocr-dot"></div>
                </div>
            </div>
            <div class="ocr-progress-bar">
                <div class="ocr-progress-fill"></div>
            </div>
        `;
        this.show();
    }

    showProgress(message, detail = '') {
        // Update existing modal content with new progress message
        const statusText = this.content.querySelector('.ocr-status-text');
        const progressText = this.content.querySelector('.ocr-progress-text');
        
        if (statusText) statusText.textContent = message;
        if (progressText) progressText.textContent = detail;
    }

    showSuccess(message = 'OCR completed successfully!', details = '', textLength = 0) {
        this.content.innerHTML = `
            <div class="ocr-success-icon">
                <div class="ocr-success-circle">
                    <div class="ocr-success-checkmark"></div>
                </div>
            </div>
            <div class="ocr-status-text">${message}</div>
            <div class="ocr-alert ocr-alert-success">
                <strong>Success!</strong> Text recognition completed successfully.
                ${textLength > 0 ? `<br>Extracted ${textLength} characters of text.` : ''}
                ${details ? `<br>${details}` : ''}
            </div>
            <button class="ocr-close-btn" onclick="window.ocrModal.hide()">Continue</button>
        `;
    }

    showError(message = 'OCR processing failed', error = '') {
        this.content.innerHTML = `
            <div class="ocr-error-icon">
                <div class="ocr-error-circle">
                    <div class="ocr-error-x"></div>
                </div>
            </div>
            <div class="ocr-status-text">${message}</div>
            <div class="ocr-alert ocr-alert-error">
                <strong>Error!</strong> ${error || 'Unable to process the image. Please check the image quality and try again.'}
            </div>
            <button class="ocr-close-btn" onclick="window.ocrModal.hide()">Try Again</button>
        `;
    }

    showWarning(message = 'OCR completed with issues', warning = '') {
        this.content.innerHTML = `
            <div class="ocr-scanner"></div>
            <div class="ocr-status-text">${message}</div>
            <div class="ocr-alert ocr-alert-warning">
                <strong>Warning!</strong> ${warning}
            </div>
            <button class="ocr-close-btn" onclick="window.ocrModal.hide()">Continue Anyway</button>
        `;
    }
}

// Enhanced Image Editor Class
class ImageEditor {
    constructor(modalId, onGeometryCreate) {
        this.modal = document.getElementById(modalId);
        this.canvas = this.modal.querySelector('#image-canvas');
        this.ctx = this.canvas.getContext('2d');
        this.onGeometryCreate = onGeometryCreate;

        this.image = null;
        this.rotation = 0;
        this.selection = null;
        this.isSelecting = false;

        // Initialize OCR Modal
        this.ocrModal = new OCRModalManager();
        window.ocrModal = this.ocrModal; // Make globally accessible

        this.initControls();
        this.initCanvasEvents();
    }

    initControls() {
        // Close button
        this.modal.querySelector('#close-image-editor-btn').addEventListener('click', () => this.hide());

        // Rotation slider
        const rotateSlider = this.modal.querySelector('#rotate-slider');
        const rotateLabel = this.modal.querySelector('label[for="rotate-slider"] span');
        
        rotateSlider.addEventListener('input', (e) => {
            this.rotation = parseInt(e.target.value, 10);
            if (rotateLabel) {
                rotateLabel.textContent = this.rotation;
            }
            this.drawImage();
        });

        // OCR button - Enhanced with better feedback
        this.modal.querySelector('#run-ocr-btn').addEventListener('click', () => this.runEnhancedOCR());

        // Geometry type selector
        this.modal.querySelector('#geometry-type-selector').addEventListener('change', (e) => {
            this.updateGeometryOptions(e.target.value);
        });

        // Create geometry button
        this.modal.querySelector('#create-geometry-btn').addEventListener('click', () => this.createGeometry());
    }

    getMousePos(canvas, evt) {
        const rect = canvas.getBoundingClientRect();
        const scaleX = canvas.width / rect.width;
        const scaleY = canvas.height / rect.height;

        return {
            x: (evt.clientX - rect.left) * scaleX,
            y: (evt.clientY - rect.top) * scaleY
        };
    }

    initCanvasEvents() {
        let startPos;

        this.canvas.addEventListener('mousedown', (e) => {
            if (!this.image) return;
            this.isSelecting = true;
            startPos = this.getMousePos(this.canvas, e);
            this.selection = { x: startPos.x, y: startPos.y, w: 0, h: 0 };
        });

        this.canvas.addEventListener('mousemove', (e) => {
            if (!this.isSelecting) return;
            const currentPos = this.getMousePos(this.canvas, e);
            this.selection.w = currentPos.x - startPos.x;
            this.selection.h = currentPos.y - startPos.y;
            this.drawImage();
        });

        this.canvas.addEventListener('mouseup', () => {
            if (!this.isSelecting) return;
            this.isSelecting = false;
            
            // Normalize the selection rectangle
            if (this.selection.w < 0) {
                this.selection.x += this.selection.w;
                this.selection.w = Math.abs(this.selection.w);
            }
            if (this.selection.h < 0) {
                this.selection.y += this.selection.h;
                this.selection.h = Math.abs(this.selection.h);
            }

            const hasValidSelection = this.selection && this.selection.w > 10 && this.selection.h > 10;
            this.modal.querySelector('#run-ocr-btn').disabled = !hasValidSelection;
            
            // Update button text based on selection
            const ocrBtn = this.modal.querySelector('#run-ocr-btn');
            if (hasValidSelection) {
                const area = Math.round(this.selection.w * this.selection.h);
                ocrBtn.textContent = `Run OCR on Selection (${this.selection.w}×${this.selection.h}px)`;
            } else {
                ocrBtn.textContent = 'Run OCR on Selection';
            }
        });
    }

    show(imageFile) {
        const reader = new FileReader();
        reader.onload = (e) => {
            this.image = new Image();
            this.image.onload = () => {
                // Set canvas resolution to match image
                this.canvas.width = this.image.width;
                this.canvas.height = this.image.height;
                this.drawImage();
                this.modal.classList.remove('hidden');
                this.modal.style.display = 'flex';
                
                // Reset controls
                this.resetControls();
                
                showNotification(`Image loaded: ${this.image.width}×${this.image.height}px`, 'info');
            };
            this.image.src = e.target.result;
        };
        reader.readAsDataURL(imageFile);
    }

    hide() {
        this.modal.classList.add('hidden');
        this.modal.style.display = 'none';
        this.reset();
    }

    reset() {
        this.image = null;
        this.rotation = 0;
        this.selection = null;
        this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
        this.resetControls();
    }

    resetControls() {
        const rotateSlider = this.modal.querySelector('#rotate-slider');
        const rotateLabel = this.modal.querySelector('label[for="rotate-slider"] span');
        const ocrResults = this.modal.querySelector('#ocr-results');
        const ocrBtn = this.modal.querySelector('#run-ocr-btn');
        const createBtn = this.modal.querySelector('#create-geometry-btn');
        const statusEl = this.modal.querySelector('#ocr-status');

        if (rotateSlider) rotateSlider.value = 0;
        if (rotateLabel) rotateLabel.textContent = '0';
        if (ocrResults) ocrResults.value = '';
        if (ocrBtn) {
            ocrBtn.disabled = true;
            ocrBtn.textContent = 'Run OCR on Selection';
        }
        if (createBtn) createBtn.disabled = true;
        if (statusEl) statusEl.textContent = '';
    }

    drawImage() {
        if (!this.image) return;

        this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
        this.ctx.save();
        this.ctx.translate(this.canvas.width / 2, this.canvas.height / 2);
        this.ctx.rotate(this.rotation * Math.PI / 180);
        this.ctx.drawImage(this.image, -this.image.width / 2, -this.image.height / 2);
        this.ctx.restore();

        // Draw selection rectangle with enhanced styling
        if (this.selection) {
            this.ctx.strokeStyle = 'rgba(33, 150, 243, 0.8)';
            this.ctx.fillStyle = 'rgba(33, 150, 243, 0.1)';
            this.ctx.lineWidth = 2;
            
            // Fill selection area
            this.ctx.fillRect(this.selection.x, this.selection.y, this.selection.w, this.selection.h);
            
            // Draw border
            this.ctx.strokeRect(this.selection.x, this.selection.y, this.selection.w, this.selection.h);
            
            // Draw corner indicators
            this.drawCornerIndicators();
        }
    }

    drawCornerIndicators() {
        if (!this.selection) return;
        
        const { x, y, w, h } = this.selection;
        const cornerSize = 8;
        
        this.ctx.fillStyle = '#2196f3';
        this.ctx.strokeStyle = 'white';
        this.ctx.lineWidth = 1;
        
        // Draw corner squares
        const corners = [
            [x, y], [x + w, y], [x, y + h], [x + w, y + h]
        ];
        
        corners.forEach(([cx, cy]) => {
            this.ctx.fillRect(cx - cornerSize/2, cy - cornerSize/2, cornerSize, cornerSize);
            this.ctx.strokeRect(cx - cornerSize/2, cy - cornerSize/2, cornerSize, cornerSize);
        });
    }

    async runEnhancedOCR() {
        if (!this.selection || this.selection.w < 10 || this.selection.h < 10) {
            showNotification('Please select an area on the image first', 'warning');
            return;
        }

        try {
            // Show processing modal
            this.ocrModal.showProcessing('Starting OCR analysis...');
            
            // Update status text in main modal
            const statusEl = this.modal.querySelector('#ocr-status');
            if (statusEl) statusEl.textContent = 'Processing...';

            // Phase 1: Initialize OCR
            await this.delay(500);
            this.ocrModal.showProgress('Initializing OCR engine...', 'Loading Tesseract.js workers...');

            // Phase 2: Crop image
            await this.delay(800);
            this.ocrModal.showProgress('Preparing image...', 'Cropping selected region...');
            const croppedCanvas = this.getCroppedCanvas();

            // Phase 3: Start OCR
            await this.delay(500);
            this.ocrModal.showProgress('Analyzing image...', 'Detecting text regions...');

            // Run actual OCR
            const { data: { text, confidence } } = await Tesseract.recognize(
                croppedCanvas,
                'eng',
                { 
                    logger: m => {
                        if (m.status === 'recognizing text') {
                            const progress = Math.round(m.progress * 100);
                            this.ocrModal.showProgress(
                                'Recognizing text...', 
                                `Progress: ${progress}% - Processing characters...`
                            );
                        }
                    }
                }
            );

            // Phase 4: Process results
            await this.delay(300);
            this.ocrModal.showProgress('Processing results...', 'Cleaning up recognized text...');

            // Update main modal
            this.modal.querySelector('#ocr-results').value = text;
            if (statusEl) statusEl.textContent = 'OCR complete.';
            this.modal.querySelector('#create-geometry-btn').disabled = !text.trim();

            // Show success with details
            const textLength = text.trim().length;
            const wordCount = text.trim().split(/\s+/).filter(w => w.length > 0).length;
            
            if (textLength > 0) {
                let details = `Found ${wordCount} words.`;
                if (confidence) {
                    details += ` Confidence: ${Math.round(confidence)}%`;
                }
                
                this.ocrModal.showSuccess(
                    'Text extraction completed!', 
                    details,
                    textLength
                );
                
                showNotification(`OCR completed: ${textLength} characters extracted`, 'success');
            } else {
                this.ocrModal.showWarning(
                    'OCR completed but no text found',
                    'The selected region may not contain readable text, or the image quality might be too low.'
                );
                
                showNotification('No text found in selected region', 'warning');
            }

        } catch (error) {
            console.error('OCR Error:', error);
            
            // Update main modal
            const statusEl = this.modal.querySelector('#ocr-status');
            if (statusEl) statusEl.textContent = 'OCR failed.';
            
            // Show error modal
            this.ocrModal.showError(
                'OCR processing failed',
                error.message || 'An unexpected error occurred during text recognition.'
            );
            
            showNotification('OCR processing failed', 'error');
        }
    }

    delay(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    getCroppedCanvas() {
        const tempCanvas = document.createElement('canvas');
        const tempCtx = tempCanvas.getContext('2d');
        tempCanvas.width = this.selection.w;
        tempCanvas.height = this.selection.h;
        
        // Draw the selected part with better quality
        tempCtx.imageSmoothingEnabled = true;
        tempCtx.imageSmoothingQuality = 'high';
        tempCtx.drawImage(
            this.canvas, 
            this.selection.x, this.selection.y, this.selection.w, this.selection.h,
            0, 0, this.selection.w, this.selection.h
        );
        
        return tempCanvas;
    }

    updateGeometryOptions(type) {
        const container = this.modal.querySelector('#geometry-options');
        let html = '';
        
        if (type === 'path') {
            html = `
                <div class="control-group">
                    <label for="path-width">Path Width (meters):</label>
                    <input type="number" id="path-width" value="100" class="form-group input" min="1" max="1000">
                    <small class="form-help">Width of the corridor along the path</small>
                </div>
            `;
        } else if (type === 'points') {
            html = `
                <div class="control-group">
                    <label for="point-radius">Point Radius (meters):</label>
                    <input type="number" id="point-radius" value="50" class="form-group input" min="1" max="500">
                    <small class="form-help">Radius of the circular area around each point</small>
                </div>
            `;
        }
        
        container.innerHTML = html;
    }

    createGeometry() {
        const ocrText = this.modal.querySelector('#ocr-results').value;
        const type = this.modal.querySelector('#geometry-type-selector').value;
        
        if (!ocrText.trim()) {
            showNotification('No OCR text available to create geometry', 'warning');
            return;
        }
        
        // Enhanced coordinate parsing with better error handling
        const lines = ocrText.split('\n').filter(line => line.trim() !== '');
        const coordinates = [];
        const errors = [];
        
        lines.forEach((line, index) => {
            try {
                // Enhanced regex for coordinate detection
                const coordPatterns = [
                    /-?\d+\.\d+/g, // Decimal coordinates
                    /\d+°\d+'[\d.]+"/g, // DMS coordinates
                    /[-+]?\d*\.?\d+/g // General numbers
                ];
                
                let matches = null;
                for (const pattern of coordPatterns) {
                    matches = line.match(pattern);
                    if (matches && matches.length >= 2) break;
                }
                
                if (matches && matches.length >= 2) {
                    const coord = matches.slice(0, 2).map(parseFloat);
                    if (!isNaN(coord[0]) && !isNaN(coord[1])) {
                        coordinates.push(coord);
                    }
                }
            } catch (error) {
                errors.push(`Line ${index + 1}: ${line}`);
            }
        });

        if (coordinates.length === 0) {
            showNotification('No valid coordinates found in OCR text', 'error');
            return;
        }

        if (errors.length > 0) {
            console.warn('Parsing errors:', errors);
            showNotification(`Found ${coordinates.length} coordinates, ${errors.length} lines had errors`, 'warning');
        }

        let geometryData = {
            type: type,
            coordinates: coordinates,
            source: 'ocr',
            selectionArea: {
                x: this.selection.x,
                y: this.selection.y,
                width: this.selection.w,
                height: this.selection.h
            }
        };

        // Add type-specific properties
        if (type === 'path') {
            const widthInput = this.modal.querySelector('#path-width');
            geometryData.width = widthInput ? parseInt(widthInput.value, 10) || 100 : 100;
        } else if (type === 'points') {
            const radiusInput = this.modal.querySelector('#point-radius');
            geometryData.radius = radiusInput ? parseInt(radiusInput.value, 10) || 50 : 50;
        }

        // Callback to parent component
        if (this.onGeometryCreate) {
            this.onGeometryCreate(geometryData);
        }
        
        showNotification(
            `Created ${type} geometry with ${coordinates.length} coordinates`, 
            'success'
        );
        
        this.hide();
    }
}

export default ImageEditor;