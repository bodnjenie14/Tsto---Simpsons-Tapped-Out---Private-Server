/**
 * Public Town Uploader Module
 * 
 * This module provides functionality for the public town uploader system,
 * allowing users to submit their town files for review by town operators.
 */
(function() {
    // Base URL for the town uploader endpoint
    const ENDPOINT_URL = '/api/submit_town_upload';
    
    /**
     * Helper function to make API requests to the town uploader endpoint
     * 
     * @param {FormData} formData - The form data to send in the request
     * @returns {Promise<Object>} - The response from the server
     */
    async function uploadTown(formData) {
        try {
            const response = await fetch(ENDPOINT_URL, {
                method: 'POST',
                body: formData
            });
            
            if (!response.ok) {
                const errorText = await response.text();
                throw new Error(`Server error: ${response.status} - ${errorText}`);
            }
            
            return await response.json();
        } catch (error) {
            console.error('Town upload request failed:', error);
            throw error;
        }
    }
    
    /**
     * Validates a town file before upload
     * 
     * @param {File} file - The town file to validate
     * @returns {Object} - Validation result with success flag and optional error message
     */
    function validateTownFile(file) {
        // Check if file exists
        if (!file) {
            return { 
                success: false, 
                message: 'No file selected' 
            };
        }
        
        // Check file extension
        if (!file.name.toLowerCase().endsWith('.pb')) {
            return { 
                success: false, 
                message: 'Invalid file type. Only .pb files are allowed.' 
            };
        }
        
        // Check file size (max 10MB)
        const maxSize = 10 * 1024 * 1024; // 10MB in bytes
        if (file.size > maxSize) {
            return { 
                success: false, 
                message: `File is too large. Maximum size is ${formatFileSize(maxSize)}.` 
            };
        }
        
        return { success: true };
    }
    
    /**
     * Format file size in human-readable format
     * 
     * @param {number} bytes - File size in bytes
     * @returns {string} - Formatted file size
     */
    function formatFileSize(bytes) {
        if (bytes < 1024) {
            return bytes + ' bytes';
        } else if (bytes < 1024 * 1024) {
            return (bytes / 1024).toFixed(2) + ' KB';
        } else {
            return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
        }
    }
    
    /**
     * Check the status of a pending town upload
     * 
     * @param {string} uploadId - The ID of the uploaded town
     * @returns {Promise<Object>} - The response from the server
     */
    async function checkTownStatus(uploadId) {
        try {
            const response = await fetch(`${ENDPOINT_URL}/status/${uploadId}`, {
                method: 'GET',
                headers: {
                    'Content-Type': 'application/json'
                }
            });
            
            if (!response.ok) {
                const errorText = await response.text();
                throw new Error(`Server error: ${response.status} - ${errorText}`);
            }
            
            return await response.json();
        } catch (error) {
            console.error('Town status check failed:', error);
            throw error;
        }
    }
    
    // Expose the functions to the global scope
    window.PublicTownUploader = {
        uploadTown,
        validateTownFile,
        formatFileSize,
        checkTownStatus
    };
})();
