// Check if user is logged in
document.addEventListener('DOMContentLoaded', function() {
    // Check if user has a valid session cookie instead of localStorage tokens
    const cookies = document.cookie.split(';');
    let hasSession = false;
    let sessionToken = '';
    
    for (let i = 0; i < cookies.length; i++) {
        const cookie = cookies[i].trim();
        if (cookie.startsWith('tsto_session=')) {
            hasSession = true;
            sessionToken = cookie.substring('tsto_session='.length);
            break;
        }
    }
    
    // Check if we're viewing as another user (admin feature)
    const urlParams = new URLSearchParams(window.location.search);
    const viewAsUserEmail = urlParams.get('view_as_user');
    
    // Determine which user to display
    const isAdminView = !!viewAsUserEmail;
    const userEmail = viewAsUserEmail || localStorage.getItem('publicUserEmail') || 'unknown';
    const token = sessionToken || localStorage.getItem('publicUserToken') || '';
    
    // If admin is viewing another user's dashboard, show an admin banner
    if (isAdminView) {
        // Add the admin banner after a short delay to ensure DOM is ready
        setTimeout(() => {
            const container = document.querySelector('.container');
            if (container) {
                // Create and insert the admin banner at the top of the page
                const adminBanner = document.createElement('div');
                adminBanner.className = 'admin-banner';
                adminBanner.innerHTML = `
                    <div style="background-color: #E53935; color: white; padding: 10px; text-align: center; margin-bottom: 20px; border-radius: 5px; font-size: 16px;">
                        <strong>ADMIN VIEW:</strong> You are viewing ${viewAsUserEmail}'s dashboard. 
                        <button onclick="window.close()" style="margin-left: 10px; background-color: white; color: #E53935; border: none; padding: 5px 10px; border-radius: 3px; cursor: pointer; font-weight: bold;">
                            Return to Admin Panel
                        </button>
                    </div>
                `;
                container.prepend(adminBanner);
                
                // Also add a floating admin indicator that stays visible when scrolling
                const floatingIndicator = document.createElement('div');
                floatingIndicator.innerHTML = `
                    <div style="position: fixed; top: 10px; right: 10px; background-color: #E53935; color: white; padding: 5px 10px; border-radius: 5px; z-index: 1000; font-weight: bold; box-shadow: 0 2px 5px rgba(0,0,0,0.2);">
                        ADMIN MODE
                    </div>
                `;
                document.body.appendChild(floatingIndicator);
            }
        }, 100);
        
        // Hide certain actions that admins shouldn't perform on behalf of users
        document.addEventListener('click', function(e) {
            if (e.target.matches('.logout-button')) {
                e.preventDefault();
                alert('You cannot log out as this user. Close this window to return to the admin panel.');
            }
        }, true);
    }
    
    // Temporarily set user display name to email until we fetch the real one
    document.getElementById('user-display-name').textContent = userEmail;
    document.getElementById('current-display-name').textContent = 'Loading...';
    
    // Fetch the display name from the main database
    fetchDisplayName(userEmail, token);
    
    // Load town information
    loadTownInfo(userEmail, token);
    
    // Load currency information
    loadCurrencyInfo(userEmail, token);
    
    // Initialize theme
    initTheme();
});

// Function to fetch display name from the main database
async function fetchDisplayName(email, token) {
    try {
        const response = await fetch(`/api/public/get_display_name?email=${encodeURIComponent(email)}`, {
            method: 'GET',
            credentials: 'include'
        });
        
        const data = await response.json();
        
        if (response.ok && data.success) {
            const displayName = data.display_name || email;
            
            // Update display name in the UI
            document.getElementById('user-display-name').textContent = displayName;
            document.getElementById('current-display-name').textContent = displayName;
            document.getElementById('display-name-field').value = displayName;
        } else {
            // If we couldn't fetch the display name, use email as fallback
            document.getElementById('current-display-name').textContent = email;
            document.getElementById('display-name-field').value = email;
            console.error('Failed to fetch display name:', data.error);
        }
    } catch (error) {
        console.error('Error fetching display name:', error);
        document.getElementById('current-display-name').textContent = email;
        document.getElementById('display-name-field').value = email;
    }
}

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
function openDisplayNameModal() {
    // Get current display name from localStorage or use email as fallback
    const email = localStorage.getItem('publicUserEmail');
    const currentDisplayName = localStorage.getItem('user_display_name') || email;
    
    // Set the current display name in the input field
    document.getElementById('display-name-input').value = currentDisplayName;
    
    // Open the modal
    document.getElementById('display-name-modal').style.display = 'block';
}

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
                'X-Filename': `${email}.pb`,
                'X-File-Size': selectedFile.size.toString() // Add file size in bytes as a header
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

function validateDeleteConfirmation() {
    const confirmationText = document.getElementById('delete-confirmation').value;
    const deleteError = document.getElementById('delete-error');
    
    if (confirmationText !== 'DELETE') {
        deleteError.style.display = 'block';
        return;
    }
    
    // Hide error message if validation passes
    deleteError.style.display = 'none';
    
    // Proceed with deletion
    confirmDeleteTown();
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

// Currency functions
async function loadCurrencyInfo(email, token) {
    const currencyContainer = document.getElementById('currency-container');
    const displayName = localStorage.getItem('user_display_name') || email;
    
    try {
        // Fetch currency information from the server
        const response = await fetch(`/api/public/currency_info?email=${encodeURIComponent(email)}`, {
            method: 'GET',
            headers: {
                'Authorization': `Bearer ${token}`
            }
        });
        
        const data = await response.json();
        
        if (response.ok) {
            if (data.hasCurrency) {
                // Store donuts in localStorage
                localStorage.setItem('donuts', data.donuts || 0);
                
                // Format the donuts value
                const donuts = parseInt(data.donuts || 0).toLocaleString();
                
                // Display the currency information
                currencyContainer.innerHTML = `
                    <div class="currency-card">
                        <div class="currency-header">
                            <h3 class="currency-title">${displayName}'s Donuts</h3>
                        </div>
                        <div class="currency-details">
                            <p><i class="fas fa-circle" style="color: #FED90F;"></i> Donuts: <strong>${donuts}</strong></p>
                        </div>
                        <div class="currency-actions">
                            <button class="action-button edit-button" onclick="openCurrencyModal()">
                                <i class="fas fa-edit"></i> Edit
                            </button>
                        </div>
                    </div>
                `;
            } else {
                // No currency file found
                currencyContainer.innerHTML = `
                    <div class="no-currency">
                        <i class="fas fa-coins"></i>
                        <p>You don't have a currency file yet!</p>
                        <button class="action-button create-button" onclick="openCurrencyModal()">
                            <i class="fas fa-plus"></i> Create Currency File
                        </button>
                    </div>
                `;
            }
        } else {
            // Error fetching currency info
            currencyContainer.innerHTML = `
                <div class="error-message">
                    <i class="fas fa-exclamation-circle"></i>
                    <p>Error: ${data.error || 'Failed to load currency information'}</p>
                </div>
            `;
        }
    } catch (error) {
        console.error('Error loading currency info:', error);
        currencyContainer.innerHTML = `
            <div class="error-message">
                <i class="fas fa-exclamation-circle"></i>
                <p>Error: ${error.message}</p>
            </div>
        `;
    }
}

function openCurrencyModal() {
    // Get donuts value from localStorage or set default
    const donuts = localStorage.getItem('donuts') || 0;
    
    // Set the value in the form
    document.getElementById('donuts-input').value = donuts;
    
    // Open the modal
    document.getElementById('currency-modal').style.display = 'block';
}

async function saveCurrency() {
    const email = localStorage.getItem('publicUserEmail');
    const token = localStorage.getItem('publicUserToken');
    
    if (!email || !token) {
        showCurrencyResponse('Please log in again', true);
        return;
    }
    
    try {
        // Show loading state
        const saveButton = document.getElementById('save-currency-button');
        const originalButtonText = saveButton.innerHTML;
        saveButton.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Saving...';
        saveButton.disabled = true;
        
        // Get the donuts value from the form
        const donuts = document.getElementById('donuts-input').value;
        
        // Send the donuts data to the server
        const response = await fetch('/api/public/save_currency', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${token}`
            },
            body: JSON.stringify({
                email,
                donuts
            })
        });
        
        const data = await response.json();
        
        // Reset button state
        saveButton.innerHTML = originalButtonText;
        saveButton.disabled = false;
        
        if (response.ok || data.success) {
            showCurrencyResponse('Currency saved successfully', false);
            closeModal('currency-modal');
            
            // Reload currency info after successful save
            setTimeout(() => {
                loadCurrencyInfo(email, token);
            }, 1000);
        } else {
            showCurrencyResponse(data.error || 'Failed to save currency', true);
        }
    } catch (error) {
        console.error('Error:', error);
        showCurrencyResponse('Error: ' + error.message, true);
        
        // Reset button state on error
        const saveButton = document.getElementById('save-currency-button');
        if (saveButton) {
            saveButton.innerHTML = '<i class="fas fa-save"></i> Save Changes';
            saveButton.disabled = false;
        }
    }
}

function showCurrencyResponse(message, isError) {
    const responseElement = document.getElementById('currency-response');
    responseElement.textContent = message;
    responseElement.className = isError ? 'response error' : 'response success';
    responseElement.style.display = 'block';
    
    // Hide the response after 5 seconds
    setTimeout(() => {
        responseElement.style.display = 'none';
    }, 5000);
}

function showDisplayNameResponse(message, isError) {
    const responseElement = document.getElementById('display-name-response');
    responseElement.textContent = message;
    responseElement.className = isError ? 'response error' : 'response success';
    responseElement.style.display = 'block';
    
    // Hide the response after 5 seconds
    setTimeout(() => {
        responseElement.style.display = 'none';
    }, 5000);
}

async function updateDisplayName() {
    const email = localStorage.getItem('publicUserEmail');
    const token = localStorage.getItem('publicUserToken');
    
    if (!email || !token) {
        showDisplayNameResponse('Please log in again', true);
        return;
    }
    
    const newDisplayName = document.getElementById('display-name-field').value.trim();
    
    // Validate display name
    if (!newDisplayName) {
        showDisplayNameResponse('Display name cannot be empty', true);
        return;
    }
    
    if (newDisplayName.length > 50) {
        showDisplayNameResponse('Display name must be less than 50 characters', true);
        return;
    }
    
    try {
        // Show loading state
        const saveButton = document.getElementById('save-name-button');
        const originalButtonText = saveButton.innerHTML;
        saveButton.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Saving...';
        saveButton.disabled = true;
        
        // Send request to update display name
        const response = await fetch('/api/public/update_display_name', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${token}`
            },
            body: JSON.stringify({
                email: email,
                display_name: newDisplayName
            })
        });
        
        const data = await response.json();
        
        // Reset button state
        saveButton.innerHTML = originalButtonText;
        saveButton.disabled = false;
        
        if (response.ok && data.success) {
            // Update display name in the UI
            document.getElementById('user-display-name').textContent = newDisplayName;
            document.getElementById('current-display-name').textContent = newDisplayName;
            
            // Show success message
            showDisplayNameResponse('Display name updated successfully', false);
        } else {
            showDisplayNameResponse(data.error || 'Failed to update display name', true);
        }
    } catch (error) {
        console.error('Error:', error);
        showDisplayNameResponse('Error: ' + error.message, true);
        
        // Reset button state on error
        const saveButton = document.getElementById('save-name-button');
        if (saveButton) {
            saveButton.innerHTML = '<i class="fas fa-save"></i> Save Name';
            saveButton.disabled = false;
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

async function saveDisplayName() {
    const email = localStorage.getItem('publicUserEmail');
    const token = localStorage.getItem('publicUserToken');
    
    if (!email || !token) {
        showResponse('Please log in again', true);
        closeModal('display-name-modal');
        return;
    }
    
    const displayNameInput = document.getElementById('display-name-input');
    const newDisplayName = displayNameInput.value.trim();
    
    // Validate display name
    if (!newDisplayName) {
        showResponse('Display name cannot be empty', true);
        return;
    }
    
    if (newDisplayName.length > 50) {
        showResponse('Display name must be less than 50 characters', true);
        return;
    }
    
    try {
        // Show loading state
        const saveButton = document.getElementById('save-display-name-button');
        const originalButtonText = saveButton.innerHTML;
        saveButton.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Saving...';
        saveButton.disabled = true;
        
        // Send request to update display name
        const response = await fetch('/api/public/update_display_name', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${token}`
            },
            body: JSON.stringify({
                email: email,
                display_name: newDisplayName
            })
        });
        
        const data = await response.json();
        
        // Reset button state
        saveButton.innerHTML = originalButtonText;
        saveButton.disabled = false;
        
        if (response.ok && data.success) {
            // Update display name in localStorage
            localStorage.setItem('user_display_name', newDisplayName);
            
            // Update display name in the UI
            document.getElementById('user-display-name').textContent = newDisplayName;
            
            // Show success message
            showResponse('Display name updated successfully', false);
            
            // Close the modal
            closeModal('display-name-modal');
        } else {
            showResponse(data.error || 'Failed to update display name', true);
        }
    } catch (error) {
        console.error('Error:', error);
        showResponse('Error: ' + error.message, true);
        
        // Reset button state on error
        const saveButton = document.getElementById('save-display-name-button');
        if (saveButton) {
            saveButton.innerHTML = '<i class="fas fa-save"></i> Save Changes';
            saveButton.disabled = false;
        }
    }
}

function handleLogout() {
    // Clear local storage
    localStorage.removeItem('publicUserToken');
    localStorage.removeItem('publicUserEmail');
    localStorage.removeItem('user_display_name');
    
    // Redirect to login page
    window.location.href = 'public_login.html';
}
