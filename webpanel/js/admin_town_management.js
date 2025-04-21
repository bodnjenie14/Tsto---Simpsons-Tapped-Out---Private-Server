// Admin Town Management functionality

// Handle admin town import for a specific user
function adminImportTown(targetUserEmail) {
    // Create a file input element
    const fileInput = document.createElement('input');
    fileInput.type = 'file';
    fileInput.accept = '.pb';
    fileInput.style.display = 'none';
    document.body.appendChild(fileInput);
    
    // Trigger file selection dialog
    fileInput.click();
    
    // Handle file selection
    fileInput.addEventListener('change', async function() {
        if (fileInput.files.length === 0) {
            document.body.removeChild(fileInput);
            return;
        }
        
        const file = fileInput.files[0];
        
        // Check file size
        const MAX_FILE_SIZE = 5 * 1024 * 1024; // 5MB
        if (file.size > MAX_FILE_SIZE) {
            alert('File is too large. Maximum size is 5MB.');
            document.body.removeChild(fileInput);
            return;
        }
        
        // Show loading indicator
        const loadingIndicator = document.createElement('div');
        loadingIndicator.className = 'loading-overlay';
        loadingIndicator.innerHTML = `
            <div class="loading-spinner"></div>
            <div class="loading-text">Uploading town for ${targetUserEmail}...</div>
        `;
        document.body.appendChild(loadingIndicator);
        
        try {
            // Create FormData
            const formData = new FormData();
            formData.append('town_file', file);
            
            // Get admin token
            const adminToken = localStorage.getItem('publicUserToken');
            
            // Send file to server
            const response = await fetch(`/api/admin/import_town?target_user=${encodeURIComponent(targetUserEmail)}`, {
                method: 'POST',
                headers: {
                    'Authorization': `Bearer ${adminToken}`,
                    'X-File-Size': file.size.toString()
                },
                body: file
            });
            
            const data = await response.json();
            
            if (response.ok && data.success) {
                alert(`Town successfully imported for user ${targetUserEmail}`);
                // Reload the page to show the updated town
                window.location.reload();
            } else {
                alert(`Error importing town: ${data.error || 'Unknown error'}`);
            }
        } catch (error) {
            console.error('Error importing town:', error);
            alert(`Error importing town: ${error.message}`);
        } finally {
            // Remove loading indicator and file input
            document.body.removeChild(loadingIndicator);
            document.body.removeChild(fileInput);
        }
    });
}

// Handle admin town export for a specific user
function adminExportTown(targetUserEmail) {
    // Get admin token
    const adminToken = localStorage.getItem('publicUserToken');
    
    // Create a download link
    const downloadLink = document.createElement('a');
    downloadLink.href = `/api/admin/export_town?target_user=${encodeURIComponent(targetUserEmail)}`;
    downloadLink.download = `${targetUserEmail}.pb`;
    
    // Add authorization header to the download request
    fetch(downloadLink.href, {
        headers: {
            'Authorization': `Bearer ${adminToken}`
        }
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.blob();
    })
    .then(blob => {
        const url = window.URL.createObjectURL(blob);
        downloadLink.href = url;
        document.body.appendChild(downloadLink);
        downloadLink.click();
        window.URL.revokeObjectURL(url);
        document.body.removeChild(downloadLink);
    })
    .catch(error => {
        console.error('Error exporting town:', error);
        alert(`Error exporting town: ${error.message}`);
    });
}

// Handle admin town deletion for a specific user
function adminDeleteTown(targetUserEmail) {
    if (!confirm(`Are you sure you want to delete ${targetUserEmail}'s town? This action cannot be undone.`)) {
        return;
    }
    
    // Get admin token
    const adminToken = localStorage.getItem('publicUserToken');
    
    // Show loading indicator
    const loadingIndicator = document.createElement('div');
    loadingIndicator.className = 'loading-overlay';
    loadingIndicator.innerHTML = `
        <div class="loading-spinner"></div>
        <div class="loading-text">Deleting town for ${targetUserEmail}...</div>
    `;
    document.body.appendChild(loadingIndicator);
    
    // Send delete request to server
    fetch('/api/admin/delete_town', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${adminToken}`
        },
        body: JSON.stringify({
            target_user: targetUserEmail
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            alert(`Town successfully deleted for user ${targetUserEmail}`);
            // Reload the page to show the updated state
            window.location.reload();
        } else {
            alert(`Error deleting town: ${data.error || 'Unknown error'}`);
        }
    })
    .catch(error => {
        console.error('Error deleting town:', error);
        alert(`Error deleting town: ${error.message}`);
    })
    .finally(() => {
        // Remove loading indicator
        document.body.removeChild(loadingIndicator);
    });
}

// Open currency editor modal for admin
function adminOpenCurrencyModal(targetUserEmail) {
    // Get admin token
    const adminToken = localStorage.getItem('publicUserToken');
    
    // Show loading indicator
    const loadingIndicator = document.createElement('div');
    loadingIndicator.className = 'loading-overlay';
    loadingIndicator.innerHTML = `
        <div class="loading-spinner"></div>
        <div class="loading-text">Loading currency for ${targetUserEmail}...</div>
    `;
    document.body.appendChild(loadingIndicator);
    
    // Fetch current currency value
    fetch(`/api/admin/get_currency?target_user=${encodeURIComponent(targetUserEmail)}`, {
        method: 'GET',
        headers: {
            'Authorization': `Bearer ${adminToken}`
        }
    })
    .then(response => response.json())
    .then(data => {
        // Remove loading indicator
        document.body.removeChild(loadingIndicator);
        
        // Get current donuts or default to 0
        const currentDonuts = data.success ? (data.donuts || 0) : 0;
        
        // Use the existing modal in the HTML rather than creating a new one
        const modal = document.getElementById('admin-currency-modal');
        if (modal) {
            // Update the modal content
            document.getElementById('admin-currency-user').textContent = targetUserEmail;
            document.getElementById('admin-donuts-input').value = currentDonuts;
            
            // Set up the save button with the correct target user email
            const saveButton = document.getElementById('admin-save-currency-button');
            // Remove any existing event listeners
            const newSaveButton = saveButton.cloneNode(true);
            saveButton.parentNode.replaceChild(newSaveButton, saveButton);
            // Add new event listener with the correct parameter
            newSaveButton.addEventListener('click', function() {
                adminSaveCurrency(targetUserEmail, parseInt(document.getElementById('admin-donuts-input').value) || 0);
            });
            
            // Show the modal
            modal.style.display = 'block';
        } else {
            // If modal doesn't exist in the HTML, create it dynamically
            const newModal = document.createElement('div');
            newModal.id = 'admin-currency-modal';
            newModal.className = 'modal';
            
            // Create modal content
            newModal.innerHTML = `
                <div class="modal-content">
                    <div class="modal-header">
                        <h3 class="modal-title">Edit User Currency</h3>
                        <span class="close-button" onclick="closeModal('admin-currency-modal')">&times;</span>
                    </div>
                    <div class="modal-body">
                        <div class="alert-box" style="background-color: #FFF3D4; border: 2px solid #FED90F; border-radius: 8px; padding: 10px; margin-bottom: 15px;">
                            <p style="margin: 0; color: #333; font-weight: bold;"><i class="fas fa-exclamation-triangle" style="color: #F57C00;"></i> Note:</p>
                            <p style="margin: 5px 0 0 0; color: #333;">Maximum allowed donuts is 100,000. Higher values will be capped.</p>
                        </div>
                        <div class="form-group">
                            <label for="admin-donuts-input">Donuts for <span id="admin-currency-user">${targetUserEmail}</span>:</label>
                            <input type="number" id="admin-donuts-input" min="0" max="100000" value="${currentDonuts}">
                        </div>
                    </div>
                    <div class="modal-footer">
                        <button onclick="closeModal('admin-currency-modal')">Cancel</button>
                        <button id="admin-save-currency-button" class="import-button" onclick="adminSaveCurrency('${targetUserEmail}', parseInt(document.getElementById('admin-donuts-input').value) || 0)">
                            <i class="fas fa-save"></i> Save Changes
                        </button>
                    </div>
                </div>
            `;
            
            // Add modal to the page
            document.body.appendChild(newModal);
            
            // Show the modal
            newModal.style.display = 'block';
        }
    })
    .catch(error => {
        // Remove loading indicator
        document.body.removeChild(loadingIndicator);
        
        console.error('Error fetching currency:', error);
        alert(`Error fetching currency: ${error.message}`);
    });
}

// Save currency from the modal
function adminSaveCurrencyFromModal(targetUserEmail) {
    // Get the donuts value from the input field
    const donutsInput = document.getElementById('admin-donuts-input');
    const donuts = parseInt(donutsInput.value) || 0;
    
    // Close the modal
    const modal = document.getElementById('admin-currency-modal');
    if (modal) {
        modal.remove();
    }
    
    // Call the save function
    adminSaveCurrency(targetUserEmail, donuts);
}

// Save admin currency (called from the modal in public_dashboard.html)
function saveAdminCurrency(targetUserEmail) {
    // Get the donuts value from the input field
    const donutsInput = document.getElementById('admin-donuts-input');
    const donuts = parseInt(donutsInput.value) || 0;
    
    // Close the modal
    closeModal('admin-currency-modal');
    
    // Call the save function
    adminSaveCurrency(targetUserEmail, donuts);
}

// Function called from the admin currency modal in public_dashboard.html
function updateAdminCurrency() {
    // Get the target user email from the span in the modal
    const targetUserEmail = document.getElementById('admin-currency-user').textContent;
    // Get the donuts value from the input field
    const donuts = parseInt(document.getElementById('admin-donuts-input').value) || 0;
    
    // Close the modal
    closeModal('admin-currency-modal');
    
    // Call the save function
    adminSaveCurrency(targetUserEmail, donuts);
}

// Handle admin currency update for a specific user
function adminSaveCurrency(targetUserEmail, donuts) {
    // Validate donuts value
    const MAX_DONUTS = 100000;
    if (isNaN(donuts) || donuts < 0) {
        donuts = 0;
    } else if (donuts > MAX_DONUTS) {
        donuts = MAX_DONUTS;
    }
    
    // Get admin token
    const adminToken = localStorage.getItem('publicUserToken');
    
    // Show loading indicator
    const loadingIndicator = document.createElement('div');
    loadingIndicator.className = 'loading-overlay';
    loadingIndicator.innerHTML = `
        <div class="loading-spinner"></div>
        <div class="loading-text">Updating currency for ${targetUserEmail}...</div>
    `;
    document.body.appendChild(loadingIndicator);
    
    // Send update request to server
    fetch('/api/admin/save_currency', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${adminToken}`
        },
        body: JSON.stringify({
            target_user: targetUserEmail,
            donuts: donuts
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            alert(`Currency successfully updated for user ${targetUserEmail}`);
            // Reload the page to show the updated currency
            window.location.reload();
        } else {
            alert(`Error updating currency: ${data.error || 'Unknown error'}`);
        }
    })
    .catch(error => {
        console.error('Error updating currency:', error);
        alert(`Error updating currency: ${error.message}`);
    })
    .finally(() => {
        // Remove loading indicator
        document.body.removeChild(loadingIndicator);
    });
}

// Add CSS for loading overlay
document.addEventListener('DOMContentLoaded', function() {
    const style = document.createElement('style');
    style.textContent = `
        .loading-overlay {
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background-color: rgba(0, 0, 0, 0.5);
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            z-index: 9999;
        }
        
        .loading-spinner {
            width: 50px;
            height: 50px;
            border: 5px solid var(--simpsons-panel);
            border-top: 5px solid var(--simpsons-blue);
            border-radius: 50%;
            animation: spin 1s linear infinite;
        }
        
        .loading-text {
            margin-top: 20px;
            color: white;
            font-size: 18px;
            font-weight: bold;
        }
        
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
    `;
    document.head.appendChild(style);
});
