import { showNotification } from './ui.js';

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

        this.initControls();
        this.initCanvasEvents();
    }

    initControls() {
        // Close button
        this.modal.querySelector('#close-image-editor-btn').addEventListener('click', () => this.hide());

        // Rotation slider
        const rotateSlider = this.modal.querySelector('#rotate-slider');
        rotateSlider.addEventListener('input', (e) => {
            this.rotation = parseInt(e.target.value, 10);
            this.modal.querySelector('#rotate-slider + label span').textContent = this.rotation;
            this.drawImage();
        });

        // OCR button
        this.modal.querySelector('#run-ocr-btn').addEventListener('click', () => this.runOCR());

        // Geometry type selector
        this.modal.querySelector('#geometry-type-selector').addEventListener('change', (e) => this.updateGeometryOptions(e.target.value));

        // Create geometry button
        this.modal.querySelector('#create-geometry-btn').addEventListener('click', () => this.createGeometry());
    }

    // FIX: Correctly calculates mouse position by scaling it to the canvas's actual resolution
    getMousePos(canvas, evt) {
        const rect = canvas.getBoundingClientRect();
        const scaleX = canvas.width / rect.width;   // relationship bitmap vs. element for X
        const scaleY = canvas.height / rect.height;  // relationship bitmap vs. element for Y

        return {
            x: (evt.clientX - rect.left) * scaleX,   // scale mouse coordinates after they have
            y: (evt.clientY - rect.top) * scaleY     // been adjusted to be relative to element
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
            this.drawImage(); // Redraw to show selection
        });

        this.canvas.addEventListener('mouseup', () => {
            if (!this.isSelecting) return;
            this.isSelecting = false;
            
            // Normalize the selection rectangle if dragged up or left
            if (this.selection.w < 0) {
                this.selection.x += this.selection.w;
                this.selection.w = Math.abs(this.selection.w);
            }
            if (this.selection.h < 0) {
                this.selection.y += this.selection.h;
                this.selection.h = Math.abs(this.selection.h);
            }

            this.modal.querySelector('#run-ocr-btn').disabled = !this.selection || this.selection.w < 1 || this.selection.h < 1;
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
        this.modal.querySelector('#rotate-slider').value = 0;
        this.modal.querySelector('#ocr-results').value = '';
        this.modal.querySelector('#run-ocr-btn').disabled = true;
        this.modal.querySelector('#create-geometry-btn').disabled = true;
    }

    drawImage() {
        if (!this.image) return;

        this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
        this.ctx.save();
        this.ctx.translate(this.canvas.width / 2, this.canvas.height / 2);
        this.ctx.rotate(this.rotation * Math.PI / 180);
        this.ctx.drawImage(this.image, -this.image.width / 2, -this.image.height / 2);
        this.ctx.restore();

        // Draw selection rectangle
        if (this.selection) {
            this.ctx.strokeStyle = 'rgba(255, 0, 0, 0.7)';
            this.ctx.lineWidth = 2;
            this.ctx.strokeRect(this.selection.x, this.selection.y, this.selection.w, this.selection.h);
        }
    }

    async runOCR() {
        if (!this.selection || this.selection.w < 1 || this.selection.h < 1) return;

        const statusEl = this.modal.querySelector('#ocr-status');
        statusEl.textContent = 'Processing OCR...';
        showNotification('Starting OCR process...', 'info');

        try {
            const { data: { text } } = await Tesseract.recognize(
                this.getCroppedCanvas(),
                'eng',
                { logger: m => console.log(m) } // Logs progress to the console
            );
            
            this.modal.querySelector('#ocr-results').value = text;
            statusEl.textContent = 'OCR complete.';
            showNotification('OCR process completed successfully.', 'success');
            this.modal.querySelector('#create-geometry-btn').disabled = false;
        } catch (error) {
            console.error('OCR Error:', error);
            statusEl.textContent = 'OCR failed. See console for details.';
            showNotification('An error occurred during OCR.', 'error');
        }
    }

    getCroppedCanvas() {
        const tempCanvas = document.createElement('canvas');
        const tempCtx = tempCanvas.getContext('2d');
        tempCanvas.width = this.selection.w;
        tempCanvas.height = this.selection.h;
        // Draw the selected part of the main canvas onto the temporary one
        tempCtx.drawImage(this.canvas, this.selection.x, this.selection.y, this.selection.w, this.selection.h, 0, 0, this.selection.w, this.selection.h);
        return tempCanvas;
    }

    updateGeometryOptions(type) {
        const container = this.modal.querySelector('#geometry-options');
        let html = '';
        if (type === 'path') {
            html = `<div class="control-group"><label for="path-width">Path Width (meters):</label><input type="number" id="path-width" value="100" class="form-group input"></div>`;
        } else if (type === 'points') {
            html = `<div class="control-group"><label for="point-radius">Point Radius (meters):</label><input type="number" id="point-radius" value="50" class="form-group input"></div>`;
        }
        container.innerHTML = html;
    }

    createGeometry() {
        const ocrText = this.modal.querySelector('#ocr-results').value;
        const type = this.modal.querySelector('#geometry-type-selector').value;
        
        // Basic parsing - assumes coordinates are separated by new lines
        const lines = ocrText.split('\n').filter(line => line.trim() !== '');
        const coordinates = lines.map(line => {
            const parts = line.match(/-?\d+\.\d+/g);
            return parts ? parts.map(parseFloat) : null;
        }).filter(coord => coord && coord.length >= 2);

        if (coordinates.length === 0) {
            showNotification('No valid coordinates found in OCR text.', 'warning');
            return;
        }

        let geometryData = {
            type: type,
            coordinates: coordinates
        };

        if (type === 'path') {
            geometryData.width = parseInt(this.modal.querySelector('#path-width').value, 10) || 100;
        } else if (type === 'points') {
            geometryData.radius = parseInt(this.modal.querySelector('#point-radius').value, 10) || 50;
        }

        if (this.onGeometryCreate) {
            this.onGeometryCreate(geometryData);
        }
        
        this.hide();
    }
}

export default ImageEditor;
