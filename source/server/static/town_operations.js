/**
 * Town Operations Client Script
 * 
 * This script provides functions to interact with the town operations endpoint
 * for loading, saving, and copying towns based on email addresses.
 */

// Configuration
const SERVER_URL = window.location.origin; // Uses the current server origin
const TOWN_OPS_ENDPOINT = `${SERVER_URL}/mh/games/bg_gameserver_plugin/townOperations/`;

/**
 * Performs a town operation (load, save, or copy)
 * 
 * @param {Object} params - The operation parameters
 * @param {string} params.operation - The operation type: 'load', 'save', or 'copy'
 * @param {string} [params.email] - The email for load/save operations
 * @param {string} [params.source] - The source email for copy operations
 * @param {string} [params.target] - The target email for copy operations
 * @param {string} params.token - The authentication token
 * @returns {Promise<Object>} - The server response
 */
async function performTownOperation(params) {
    if (!params.operation) {
        throw new Error('Operation type is required');
    }

    if (!params.token) {
        throw new Error('Authentication token is required');
    }

    // Validate parameters based on operation
    if (params.operation === 'load' || params.operation === 'save') {
        if (!params.email) {
            throw new Error('Email is required for load/save operations');
        }
    } else if (params.operation === 'copy') {
        if (!params.source || !params.target) {
            throw new Error('Source and target emails are required for copy operations');
        }
    } else {
        throw new Error('Invalid operation type. Must be "load", "save", or "copy"');
    }

    // Prepare request body
    const requestBody = {
        operation: params.operation
    };

    // Add operation-specific parameters
    if (params.operation === 'load' || params.operation === 'save') {
        requestBody.email = params.email;
    } else if (params.operation === 'copy') {
        requestBody.source = params.source;
        requestBody.target = params.target;
    }

    try {
        const response = await fetch(TOWN_OPS_ENDPOINT, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'mh_auth_params': params.token
            },
            body: JSON.stringify(requestBody)
        });

        const data = await response.json();
        
        if (!response.ok) {
            throw new Error(data.error || 'Operation failed');
        }
        
        return data;
    } catch (error) {
        console.error('Town operation failed:', error);
        throw error;
    }
}

/**
 * Loads a town by email
 * 
 * @param {string} email - The email of the town to load
 * @param {string} token - The authentication token
 * @returns {Promise<Object>} - The server response
 */
async function loadTown(email, token) {
    return performTownOperation({
        operation: 'load',
        email,
        token
    });
}

/**
 * Saves the current town under a specific email
 * 
 * @param {string} email - The email to save the town as
 * @param {string} token - The authentication token
 * @returns {Promise<Object>} - The server response
 */
async function saveTownAs(email, token) {
    return performTownOperation({
        operation: 'save',
        email,
        token
    });
}

/**
 * Copies a town from one email to another
 * 
 * @param {string} sourceEmail - The source email to copy from
 * @param {string} targetEmail - The target email to copy to
 * @param {string} token - The authentication token
 * @returns {Promise<Object>} - The server response
 */
async function copyTown(sourceEmail, targetEmail, token) {
    return performTownOperation({
        operation: 'copy',
        source: sourceEmail,
        target: targetEmail,
        token
    });
}

// Export functions for use in other scripts
window.TownOperations = {
    loadTown,
    saveTownAs,
    copyTown,
    performTownOperation
};

console.log('Town Operations client script loaded successfully');
