// Check if user is logged in
document.addEventListener('DOMContentLoaded', function() {
    // Check if user is logged in
    const token = localStorage.getItem('publicUserToken');
    const email = localStorage.getItem('publicUserEmail');
    
    if (!token || !email) {
        // Redirect to login page if not logged in
        window.location.href = 'public_login.html';
        return;
    }
    
    // Set user display name
    const displayName = localStorage.getItem('user_display_name') || email;
    document.getElementById('user-display-name').textContent = displayName;
    
    // Load town information
    loadTownInfo(email, token);
    
    // Initialize theme
    initTheme();
});

// Theme handling
function initTheme() {
    const isDarkMode = localStorage.getItem('darkMode') === 'true';
    if (isDarkMode) {
        document.body.classList.add('dark-mode');
        document.querySelector('.theme-toggle i').classList.remove('fa-moon');
        document.querySelector('.theme-toggle i').classList.add('fa-sun');
    }
}

function toggleTheme() {
    const isDarkMode = document.body.classList.toggle('dark-mode');
    const icon = document.querySelector('.theme-toggle i');
    
    if (isDarkMode) {
        icon.classList.remove('fa-moon');
        icon.classList.add('fa-sun');
    } else {
        icon.classList.remove('fa-sun');
        icon.classList.add('fa-moon');
    }
    
    localStorage.setItem('darkMode', isDarkMode);
}

// Town management functions
async function loadTownInfo(email, token) {
    // If no token or email is provided, get them from localStorage
    if (!email || !token) {
        email = localStorage.getItem('publicUserEmail');
        token = localStorage.getItem('publicUserToken');
        
        if (!email || !token) {
            window.location.href = 'public_login.html';
            return;
        }
    }
    const townContainer = document.getElementById('town-container');
    // Make sure to get the display name from localStorage or fall back to email
    const displayName = localStorage.getItem('user_display_name') || email;
    
    try {
        // Fetch town information from the server
        const response = await fetch(`/api/public/town_info?email=${encodeURIComponent(email)}`, {
            method: 'GET',
            headers: {
                'Authorization': `Bearer ${token}`
            }
        });
        
        const data = await response.json();
        
        if (response.ok) {
            if (data.hasTown) {
                // Display town information
                const townSize = formatFileSize(data.size || 0);
                const lastModified = new Date(data.lastModified || Date.now()).toLocaleString();
                
                // Store town info in localStorage for use in modals
                localStorage.setItem('town_size', townSize);
                localStorage.setItem('town_last_modified', lastModified);
                
                townContainer.innerHTML = `
                    <div class="town-card">
                        <div class="town-header">
                            <h3 class="town-title">${displayName}'s Springfield</h3>
                            <span class="town-size">${townSize}</span>
                        </div>
                        <div class="town-details">
                            <p><i class="fas fa-calendar-alt"></i> Last modified: ${lastModified}</p>
                            <p><i class="fas fa-database"></i> Town size: <strong>${townSize}</strong></p>
                        </div>
                        <div class="town-actions">
                            <button class="action-button import-button" onclick="openModal('import-modal')">
                                <i class="fas fa-file-upload"></i> Import Town
                            </button>
                            <button class="action-button export-button" onclick="exportTown()">
                                <i class="fas fa-file-download"></i> Export Town
                            </button>
                            <button class="action-button delete-button" onclick="openModal('delete-modal')">
                                <i class="fas fa-trash-alt"></i> Delete Town
                            </button>
                        </div>
                    </div>
                `;
            } else {
                // No town found, show upload option
                townContainer.innerHTML = `
                    <div class="no-town">
                        <i class="fas fa-home"></i>
                        <p>You don't have a town yet!</p>
                        <button class="import-button" onclick="openModal('import-modal')">
                            <i class="fas fa-file-upload"></i> Import Town
                        </button>
                    </div>
                `;
            }
        } else {
            // Error fetching town info
            showResponse(data.error || 'Failed to load town information', true);
            townContainer.innerHTML = `
                <div class="no-town">
                    <i class="fas fa-exclamation-triangle"></i>
                    <p>Error loading town information.</p>
                    <button onclick="loadTownInfo('${email}', '${token}')">
                        <i class="fas fa-sync"></i> Retry
                    </button>
                </div>
            `;
        }
    } catch (error) {
        console.error('Error:', error);
        showResponse('An error occurred while loading town information', true);
        townContainer.innerHTML = `
            <div class="no-town">
                <i class="fas fa-exclamation-triangle"></i>
                <p>Error: ${error.message}</p>
                <button onclick="loadTownInfo('${email}', '${token}')">
                    <i class="fas fa-sync"></i> Retry
                </button>
            </div>
        `;
    }
}

// Modal handling
function openModal(modalId) {
    const modal = document.getElementById(modalId);
    if (modal) {
        modal.style.display = 'block';
        
        // Handle import modal specific actions
        if (modalId === 'import-modal') {
            // Reset file input
            const fileInput = document.getElementById('town-file-input');
            if (fileInput) {
                fileInput.value = '';
                document.getElementById('selected-file').textContent = '';
                fileInput.addEventListener('change', handleFileSelect);
            }
            
            // Get user info
            const email = localStorage.getItem('publicUserEmail');
            const token = localStorage.getItem('publicUserToken');
            const displayName = localStorage.getItem('user_display_name') || email;
            
            // Update the title in the modal
            document.getElementById('import-town-title').textContent = `${displayName}'s Springfield`;
            
            // Set default values initially
            document.getElementById('import-town-size').textContent = '0 Bytes';
            document.getElementById('import-last-modified').textContent = 'N/A';
            document.getElementById('import-town-size-detail').textContent = '0 Bytes';
            
            // Check the server directly for town existence
            fetch(`/api/public/town_info?email=${encodeURIComponent(email)}`, {
                method: 'GET',
                headers: {
                    'Authorization': `Bearer ${token}`
                }
            })
            .then(response => response.json())
            .then(data => {
                if (data.hasTown) {
                    // Town exists, update with actual values
                    const townSize = formatFileSize(data.size || 0);
                    const lastModified = new Date(data.lastModified || Date.now()).toLocaleString();
                    
                    document.getElementById('import-town-size').textContent = townSize;
                    document.getElementById('import-last-modified').textContent = lastModified;
                    document.getElementById('import-town-size-detail').textContent = townSize;
                }
            })
            .catch(error => {
                console.error('Error checking town existence:', error);
            });
        }
    }
}

function closeModal(modalId) {
    document.getElementById(modalId).style.display = 'none';
}

// Close modal if clicking outside of it
window.onclick = function(event) {
    if (event.target.classList.contains('modal')) {
        event.target.style.display = 'none';
    }
};

// File handling
let selectedFile = null;

function handleFileSelect(event) {
    const files = event.target.files;
    if (files.length > 0) {
        const file = files[0];
        const maxSize = 5 * 1024 * 1024; // 5MB in bytes
        
        // Check file extension
        if (!file.name.toLowerCase().endsWith('.pb')) {
            showResponse('Only .pb files are allowed', true);
            event.target.value = '';
            document.getElementById('selected-file').innerHTML = '';
            selectedFile = null;
            return;
        }
        
        // Check file size
        if (file.size > maxSize) {
            showResponse('File size exceeds the maximum limit of 5MB', true);
            event.target.value = '';
            document.getElementById('selected-file').innerHTML = '';
            selectedFile = null;
            return;
        }
        
        selectedFile = file;
        const fileSize = formatFileSize(selectedFile.size);
        
        // Update file info display
        const selectedFileElement = document.getElementById('selected-file');
        if (selectedFileElement) {
            selectedFileElement.innerHTML = `
                <div class="file-info">
                    <i class="fas fa-file"></i> ${selectedFile.name} (${fileSize})
                </div>
            `;
        }
    }
}

async function uploadTownFile() {
    const email = localStorage.getItem('publicUserEmail');
    const token = localStorage.getItem('publicUserToken');
    
    if (!email || !token) {
        showResponse('Please log in again', true);
        return;
    }
    
    if (!selectedFile) {
        showResponse('Please select a file first', true);
        return;
    }
    
    try {
        // Show loading state
        const uploadButton = document.getElementById('upload-button');
        const originalButtonText = uploadButton.innerHTML;
        uploadButton.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Uploading...';
        uploadButton.disabled = true;
        
        const formData = new FormData();
        formData.append('town_file', selectedFile);
        
        // Don't set Content-Type header - browser will set it automatically with boundary for FormData
        const response = await fetch(`/api/public/import_town`, {
            method: 'POST',
            headers: {
                'Authorization': `Bearer ${token}`,
                'X-Filename': `${email}.pb`
            },
            body: selectedFile
        });
        
        const data = await response.json();
        
        // Reset button state
        uploadButton.innerHTML = originalButtonText;
        uploadButton.disabled = false;
        
        if (response.ok || data.success) {
            showResponse('Town imported successfully', false);
            closeModal('import-modal');
            
            // Reload town info after successful import
            setTimeout(() => {
                loadTownInfo(email, token);
            }, 1000);
        } else {
            showResponse(data.error || 'Failed to import town', true);
        }
    } catch (error) {
        console.error('Error:', error);
        showResponse('Error: ' + error.message, true);
        
        // Reset button state on error
        const uploadButton = document.getElementById('upload-button');
        uploadButton.innerHTML = '<i class="fas fa-upload"></i> Upload';
        uploadButton.disabled = false;
    }
}

async function exportTown() {
    const email = localStorage.getItem('publicUserEmail');
    const token = localStorage.getItem('publicUserToken');
    
    if (!email || !token) {
        showResponse('Please log in again', true);
        return;
    }
    
    try {
        // Show loading state
        const exportButton = document.querySelector('.export-button');
        if (exportButton) {
            const originalButtonText = exportButton.innerHTML;
            exportButton.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Exporting...';
            exportButton.disabled = true;
            
            // Reset button after 3 seconds regardless of outcome
            setTimeout(() => {
                exportButton.innerHTML = originalButtonText;
                exportButton.disabled = false;
            }, 3000);
        }
        
        // Create a direct download link
        const downloadLink = document.createElement('a');
        downloadLink.style.display = 'none';
        document.body.appendChild(downloadLink);
        
        // Set the download URL with authentication
        const exportUrl = `/api/public/export_town?email=${encodeURIComponent(email)}`;
        
        // Fetch the file directly
        const response = await fetch(exportUrl, {
            method: 'GET',
            headers: {
                'Authorization': `Bearer ${token}`
            }
        });
        
        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.error || 'Failed to export town');
        }
        
        // Get the file content
        const blob = await response.blob();
        
        // Create a download link
        const url = window.URL.createObjectURL(blob);
        downloadLink.href = url;
        downloadLink.download = `${email}.pb`;
        downloadLink.click();
        
        // Clean up
        window.URL.revokeObjectURL(url);
        document.body.removeChild(downloadLink);
        
        showResponse('Town exported successfully!', false);
    } catch (error) {
        console.error('Error:', error);
        showResponse('Error: ' + error.message, true);
    }
}

async function confirmDeleteTown() {
    const email = localStorage.getItem('publicUserEmail');
    const token = localStorage.getItem('publicUserToken');
    
    if (!email || !token) {
        showResponse('Please log in again', true);
        closeModal('delete-modal');
        return;
    }
    
    try {
        // Show loading state
        const deleteButton = document.querySelector('.delete-button');
        if (deleteButton) {
            const originalButtonText = deleteButton.innerHTML;
            deleteButton.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Deleting...';
            deleteButton.disabled = true;
        }
        
        // Use the public delete town endpoint
        const response = await fetch('/api/public/delete_town', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${token}`
            },
            body: JSON.stringify({ email })
        });
        
        // Reset button state
        if (deleteButton) {
            deleteButton.innerHTML = '<i class="fas fa-trash-alt"></i> Delete';
            deleteButton.disabled = false;
        }
        
        const data = await response.json();
        
        if (response.ok || data.success) {
            showResponse('Town deleted successfully', false);
            closeModal('delete-modal');
            
            // Clear cached town info
            localStorage.removeItem('town_size');
            localStorage.removeItem('town_last_modified');
            
            // Reload town info after successful deletion
            setTimeout(() => {
                loadTownInfo(email, token);
            }, 1000);
        } else {
            showResponse(data.error || 'Failed to delete town', true);
            closeModal('delete-modal');
        }
    } catch (error) {
        console.error('Error:', error);
        showResponse('Error: ' + error.message, true);
        closeModal('delete-modal');
        
        // Reset button state on error
        const deleteButton = document.querySelector('.delete-button');
        if (deleteButton) {
            deleteButton.innerHTML = '<i class="fas fa-trash-alt"></i> Delete';
            deleteButton.disabled = false;
        }
    }
}

// Utility functions
function formatFileSize(bytes) {
    if (bytes === 0) return '0 Bytes';
    
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

function showResponse(message, isError) {
    const responseElement = document.getElementById('response');
    responseElement.textContent = message;
    responseElement.className = 'response ' + (isError ? 'error' : 'success');
    responseElement.style.display = 'block';
    
    // Auto-hide after 5 seconds
    setTimeout(() => {
        responseElement.style.display = 'none';
    }, 5000);
}

function handleLogout() {
    // Clear local storage
    localStorage.removeItem('publicUserToken');
    localStorage.removeItem('publicUserEmail');
    localStorage.removeItem('user_display_name');
    
    // Redirect to login page
    window.location.href = 'public_login.html';
}
