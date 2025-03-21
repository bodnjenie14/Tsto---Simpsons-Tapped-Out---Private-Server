/**
 * Town Operations Module
 * 
 * This module provides functions for interacting with the town operations endpoint
 * to load, save, and copy towns in the TSTO Private Server.
 */
(function() {
    // Base URL for the town operations endpoint
    const ENDPOINT_URL = '/mh/games/bg_gameserver_plugin/townOperations/';
    
    /**
     * Helper function to make API requests to the town operations endpoint
     * 
     * @param {Object} data - The data to send in the request
     * @returns {Promise<Object>} - The response from the server
     */
    async function makeRequest(data) {
        try {
            // Get the nucleus token from localStorage if available
            const nucleusToken = localStorage.getItem('nucleus_token');
            
            const headers = {
                'Content-Type': 'application/json'
            };
            
            // Add the nucleus_token header if available
            if (nucleusToken) {
                headers['nucleus_token'] = nucleusToken;
            }
            
            const response = await fetch(ENDPOINT_URL, {
                method: 'POST',
                headers: headers,
                body: JSON.stringify(data)
            });
            
            if (!response.ok) {
                const errorText = await response.text();
                throw new Error(`Server error: ${response.status} - ${errorText}`);
            }
            
            return await response.json();
        } catch (error) {
            console.error('Town operations request failed:', error);
            throw error;
        }
    }
    
    /**
     * Load a town from the specified email
     * 
     * @param {string} email - The email associated with the town to load
     * @returns {Promise<Object>} - The response from the server
     */
    async function loadTown(email) {
        const data = {
            operation: 'load',
            email: email
        };
        
        return await makeRequest(data);
    }
    
    /**
     * Save the current town under the specified email
     * 
     * @param {string} email - The email to save the town as
     * @returns {Promise<Object>} - The response from the server
     */
    async function saveTownAs(email) {
        const data = {
            operation: 'save',
            email: email
        };
        
        return await makeRequest(data);
    }
    
    /**
     * Copy a town from one email to another
     * 
     * @param {string} sourceEmail - The email to copy from
     * @param {string} targetEmail - The email to copy to
     * @returns {Promise<Object>} - The response from the server
     */
    async function copyTown(sourceEmail, targetEmail) {
        const data = {
            operation: 'copy',
            sourceEmail: sourceEmail,
            targetEmail: targetEmail
        };
        
        return await makeRequest(data);
    }
    
    /**
     * Import a town file from the user's PC
     * 
     * @param {string} email - The email to associate with the town (optional for non-logged-in users)
     * @param {File} file - The town file to import
     * @returns {Promise<Object>} - The response from the server
     */
    async function importTown(email, file) {
        // First, we need to upload the file to a temporary location on the server
        const formData = new FormData();
        formData.append('file', file);
        
        try {
            // Get the nucleus token from localStorage if available
            const nucleusToken = localStorage.getItem('nucleus_token');
            
            const headers = {};
            
            // Add the nucleus_token header if available
            if (nucleusToken) {
                headers['nucleus_token'] = nucleusToken;
            }
            
            // Upload the file
            const uploadResponse = await fetch('/upload_town_file', {
                method: 'POST',
                headers: headers,
                body: formData
            });
            
            if (!uploadResponse.ok) {
                const errorText = await uploadResponse.text();
                throw new Error(`File upload failed: ${uploadResponse.status} - ${errorText}`);
            }
            
            const uploadResult = await uploadResponse.json();
            
            if (!uploadResult.success || !uploadResult.filePath) {
                throw new Error('File upload failed: ' + (uploadResult.message || 'Unknown error'));
            }
            
            console.log('File uploaded successfully, path:', uploadResult.filePath);
            
            // Now import the town using the temporary file path
            const data = {
                operation: 'import',
                filePath: uploadResult.filePath
            };
            
            // Only include email if it's provided (for logged-in users)
            if (email && email.trim() !== '') {
                data.email = email;
            }
            
            console.log('Sending import request with data:', data);
            
            const importResponse = await makeRequest(data);
            console.log('Import response:', importResponse);
            return importResponse;
        } catch (error) {
            console.error('Town import failed:', error);
            throw error;
        }
    }
    
    // Expose the functions to the global scope
    window.TownOperations = {
        loadTown,
        saveTownAs,
        copyTown,
        importTown
    };
})();
