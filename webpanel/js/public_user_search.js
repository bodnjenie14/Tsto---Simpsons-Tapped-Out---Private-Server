// Public User Search functionality

// Search for public users
function searchPublicUsers() {
    // Get search parameters
    const searchField = document.getElementById('public-search-field').value;
    const searchTerm = document.getElementById('public-search-term').value.trim();
    
    // Validate search term
    if (!searchTerm) {
        const responseElement = document.getElementById('public-search-response');
        responseElement.textContent = 'Please enter a search term';
        responseElement.className = 'response error';
        responseElement.style.display = 'block';
        return;
    }
    
    // Show loading spinner
    document.getElementById('public-loading').style.display = 'block';
    
    // Clear any previous response
    const responseElement = document.getElementById('public-search-response');
    responseElement.style.display = 'none';
    responseElement.textContent = '';
    responseElement.className = 'response';
    
    // Clear previous results
    document.getElementById('public-results-body').innerHTML = '';
    document.getElementById('public-results-table').style.display = 'none';
    
    // Add timestamp to prevent caching
    const timestamp = new Date().getTime();
    
    // Make API call to search public users
    console.log(`Searching for public users with field: ${searchField}, term: ${searchTerm}`);
    fetch(`/api/search-public-users?field=${encodeURIComponent(searchField)}&term=${encodeURIComponent(searchTerm)}&_=${timestamp}`, {
        method: 'GET',
        credentials: 'include'
    })
    .then(response => {
        console.log("Search response status:", response.status);
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        // Hide loading spinner
        document.getElementById('public-loading').style.display = 'none';
        
        console.log("Search response data:", data);
        
        if (data.success) {
            if (data.users && data.users.length > 0) {
                // Display the results
                displayPublicSearchResults(data.users);
            } else {
                // No users found
                responseElement.textContent = 'No public users found matching your search criteria';
                responseElement.classList.add('info');
                responseElement.style.display = 'block';
            }
        } else {
            // Error message
            responseElement.textContent = data.error || 'An error occurred while searching for public users';
            responseElement.classList.add('error');
            responseElement.style.display = 'block';
        }
    })
    .catch(error => {
        // Hide loading spinner
        document.getElementById('public-loading').style.display = 'none';
        
        // Show error message
        console.error('Error searching for public users:', error);
        responseElement.textContent = 'An error occurred while searching for public users. Please try again.';
        responseElement.classList.add('error');
        responseElement.style.display = 'block';
    });
}

// Display public search results in the table
function displayPublicSearchResults(users) {
    const resultsBody = document.getElementById('public-results-body');
    resultsBody.innerHTML = '';
    
    users.forEach(user => {
        const row = document.createElement('tr');
        
        // Add email cell
        const emailCell = document.createElement('td');
        emailCell.textContent = user.email || 'N/A';
        row.appendChild(emailCell);
        
        // Add display_name cell
        const displayNameCell = document.createElement('td');
        displayNameCell.textContent = user.display_name || 'N/A';
        row.appendChild(displayNameCell);
        
        // Add tsto_email cell
        const tstoEmailCell = document.createElement('td');
        tstoEmailCell.textContent = user.tsto_email || 'N/A';
        row.appendChild(tstoEmailCell);
        
        // Add registration_date cell
        const registrationDateCell = document.createElement('td');
        if (user.registration_date) {
            const date = new Date(user.registration_date * 1000);
            registrationDateCell.textContent = date.toLocaleString();
        } else {
            registrationDateCell.textContent = 'N/A';
        }
        row.appendChild(registrationDateCell);
        
        // Add last_login cell
        const lastLoginCell = document.createElement('td');
        if (user.last_login) {
            const date = new Date(user.last_login * 1000);
            lastLoginCell.textContent = date.toLocaleString();
        } else {
            lastLoginCell.textContent = 'N/A';
        }
        row.appendChild(lastLoginCell);
        
        // Add is_verified cell
        const verifiedCell = document.createElement('td');
        verifiedCell.textContent = user.is_verified ? 'Yes' : 'No';
        row.appendChild(verifiedCell);
        
        // Add failed_login_attempts cell
        const failedAttemptsCell = document.createElement('td');
        failedAttemptsCell.textContent = user.failed_login_attempts || '0';
        row.appendChild(failedAttemptsCell);
        
        // Add account_locked cell
        const accountLockedCell = document.createElement('td');
        accountLockedCell.textContent = user.account_locked ? 'Yes' : 'No';
        row.appendChild(accountLockedCell);
        
        // Add actions cell
        const actionsCell = document.createElement('td');
        
        // Edit button
        const editButton = document.createElement('button');
        editButton.className = 'action-btn edit-btn';
        editButton.title = 'Edit User';
        editButton.innerHTML = '<i class="fas fa-edit"></i>';
        editButton.onclick = function() {
            editPublicUser(user.email);
        };
        actionsCell.appendChild(editButton);
        
        // View as User button
        const viewAsUserButton = document.createElement('button');
        viewAsUserButton.className = 'action-btn view-btn';
        viewAsUserButton.title = 'View as User';
        viewAsUserButton.innerHTML = '<i class="fas fa-eye"></i>';
        viewAsUserButton.onclick = function() {
            viewAsUser(user.email);
        };
        actionsCell.appendChild(viewAsUserButton);
        
        // Delete button
        const deleteButton = document.createElement('button');
        deleteButton.className = 'action-btn delete-btn';
        deleteButton.title = 'Delete User';
        deleteButton.innerHTML = '<i class="fas fa-trash-alt"></i>';
        deleteButton.onclick = function() {
            confirmDeletePublicUser(user.email);
        };
        actionsCell.appendChild(deleteButton);
        
        row.appendChild(actionsCell);
        
        // Add the row to the table
        resultsBody.appendChild(row);
    });
    
    // Show the results table
    document.getElementById('public-results-table').style.display = 'table';
}

// Edit public user
function editPublicUser(email) {
    // Show loading spinner
    document.getElementById('public-loading').style.display = 'block';
    
    // Make API call to get user details
    fetch(`/api/get-public-user?email=${encodeURIComponent(email)}`, {
        method: 'GET',
        credentials: 'include'
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        // Hide loading spinner
        document.getElementById('public-loading').style.display = 'none';
        
        if (data.success && data.user) {
            // Populate the edit form
            document.getElementById('public-edit-email').value = data.user.email || '';
            document.getElementById('public-edit-display-name').value = data.user.display_name || '';
            document.getElementById('public-edit-tsto-email').value = data.user.tsto_email || '';
            document.getElementById('public-edit-verified').value = data.user.is_verified ? '1' : '0';
            document.getElementById('public-edit-failed-attempts').value = data.user.failed_login_attempts || '0';
            document.getElementById('public-edit-account-locked').value = data.user.account_locked ? '1' : '0';
            
            // Show the edit form
            document.getElementById('public-edit-form').style.display = 'block';
            
            // Scroll to the edit form
            document.getElementById('public-edit-form').scrollIntoView({ behavior: 'smooth' });
        } else {
            // Show error message
            const responseElement = document.getElementById('public-search-response');
            responseElement.textContent = data.error || 'An error occurred while retrieving user details';
            responseElement.className = 'response error';
            responseElement.style.display = 'block';
        }
    })
    .catch(error => {
        // Hide loading spinner
        document.getElementById('public-loading').style.display = 'none';
        
        // Show error message
        console.error('Error retrieving user details:', error);
        const responseElement = document.getElementById('public-search-response');
        responseElement.textContent = 'An error occurred while retrieving user details. Please try again.';
        responseElement.className = 'response error';
        responseElement.style.display = 'block';
    });
}

// Save public user changes
function savePublicUserChanges() {
    // Get form values
    const email = document.getElementById('public-edit-email').value;
    const displayName = document.getElementById('public-edit-display-name').value;
    const tstoEmail = document.getElementById('public-edit-tsto-email').value;
    const isVerified = document.getElementById('public-edit-verified').value === '1';
    const failedAttempts = document.getElementById('public-edit-failed-attempts').value;
    const accountLocked = document.getElementById('public-edit-account-locked').value === '1';
    
    // Create user object
    const user = {
        email: email,
        display_name: displayName,
        tsto_email: tstoEmail,
        is_verified: isVerified,
        failed_login_attempts: parseInt(failedAttempts),
        account_locked: accountLocked
    };
    
    // Show loading spinner
    document.getElementById('public-loading').style.display = 'block';
    
    // Make API call to update user
    fetch('/api/update-public-user', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(user),
        credentials: 'include'
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        // Hide loading spinner
        document.getElementById('public-loading').style.display = 'none';
        
        if (data.success) {
            // Hide the edit form
            document.getElementById('public-edit-form').style.display = 'none';
            
            // Show success message
            const responseElement = document.getElementById('public-search-response');
            responseElement.textContent = 'User updated successfully';
            responseElement.className = 'response success';
            responseElement.style.display = 'block';
            
            // Refresh the search results
            if (document.getElementById('public-search-term').value.trim()) {
                searchPublicUsers();
            } else {
                showAllPublicUsers();
            }
        } else {
            // Show error message
            const responseElement = document.getElementById('public-search-response');
            responseElement.textContent = data.error || 'An error occurred while updating the user';
            responseElement.className = 'response error';
            responseElement.style.display = 'block';
        }
    })
    .catch(error => {
        // Hide loading spinner
        document.getElementById('public-loading').style.display = 'none';
        
        // Show error message
        console.error('Error updating user:', error);
        const responseElement = document.getElementById('public-search-response');
        responseElement.textContent = 'An error occurred while updating the user. Please try again.';
        responseElement.className = 'response error';
        responseElement.style.display = 'block';
    });
}

// Confirm delete public user
function confirmDeletePublicUser(email) {
    if (confirm(`Are you sure you want to delete the public user with email: ${email}?`)) {
        deletePublicUser(email);
    }
}

// View as user function
function viewAsUser(email) {
    // Show loading spinner
    document.getElementById('public-loading').style.display = 'block';
    
    // First authenticate with the admin endpoint
    fetch(`/api/admin/view_user?email=${encodeURIComponent(email)}`, {
        method: 'GET',
        credentials: 'include' // Include cookies for admin authentication
    })
    .then(response => {
        // Hide loading spinner
        document.getElementById('public-loading').style.display = 'none';
        
        if (!response.ok) {
            if (response.status === 403) {
                alert('Admin access required to view user dashboards');
            } else {
                alert('Authentication error. Please try again.');
            }
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        if (data.success) {
            // Store the temporary token and email in localStorage
            localStorage.setItem('publicUserToken', data.token);
            localStorage.setItem('publicUserEmail', data.email);
            
            // Open the public dashboard in a new tab
            window.open(`public_dashboard.html?view_as_user=${encodeURIComponent(email)}`, '_blank');
        } else {
            alert(data.error || 'Failed to authenticate as admin');
        }
    })
    .catch(error => {
        console.error('Error authenticating as admin:', error);
    });
}

// Delete public user
function deletePublicUser(email) {
    // Show loading spinner
    document.getElementById('public-loading').style.display = 'block';
    
    // Make API call to delete user
    fetch('/api/delete-public-user', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({ email: email }),
        credentials: 'include'
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        // Hide loading spinner
        document.getElementById('public-loading').style.display = 'none';
        
        if (data.success) {
            // Hide the edit form if it's open
            document.getElementById('public-edit-form').style.display = 'none';
            
            // Show success message
            const responseElement = document.getElementById('public-search-response');
            responseElement.textContent = 'User deleted successfully';
            responseElement.className = 'response success';
            responseElement.style.display = 'block';
            
            // Refresh the search results
            if (document.getElementById('public-search-term').value.trim()) {
                searchPublicUsers();
            } else {
                showAllPublicUsers();
            }
        } else {
            // Show error message
            const responseElement = document.getElementById('public-search-response');
            responseElement.textContent = data.error || 'An error occurred while deleting the user';
            responseElement.className = 'response error';
            responseElement.style.display = 'block';
        }
    })
    .catch(error => {
        // Hide loading spinner
        document.getElementById('public-loading').style.display = 'none';
        
        // Show error message
        console.error('Error deleting user:', error);
        const responseElement = document.getElementById('public-search-response');
        responseElement.textContent = 'An error occurred while deleting the user. Please try again.';
        responseElement.className = 'response error';
        responseElement.style.display = 'block';
    });
}

// Show all public users
function showAllPublicUsers() {
    // Show loading spinner
    document.getElementById('public-loading').style.display = 'block';
    
    // Clear any previous response
    const responseElement = document.getElementById('public-search-response');
    responseElement.style.display = 'none';
    responseElement.textContent = '';
    responseElement.className = 'response';
    
    // Clear previous results
    document.getElementById('public-results-body').innerHTML = '';
    document.getElementById('public-results-table').style.display = 'none';
    
    // Add timestamp to prevent caching
    const timestamp = new Date().getTime();
    
    // Make API call to get all users
    fetch(`/api/get-all-public-users?_=${timestamp}`, {
        method: 'GET',
        credentials: 'include'
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        // Hide loading spinner
        document.getElementById('public-loading').style.display = 'none';
        
        if (data.success) {
            if (data.users && data.users.length > 0) {
                // Display the results
                displayPublicSearchResults(data.users);
            } else {
                // No users found
                responseElement.textContent = 'No public users found in the database';
                responseElement.classList.add('info');
                responseElement.style.display = 'block';
            }
        } else {
            // Error message
            responseElement.textContent = data.error || 'An error occurred while retrieving users';
            responseElement.classList.add('error');
            responseElement.style.display = 'block';
        }
    })
    .catch(error => {
        // Hide loading spinner
        document.getElementById('public-loading').style.display = 'none';
        
        // Show error message
        console.error('Error retrieving users:', error);
        responseElement.textContent = 'An error occurred while retrieving users. Please try again.';
        responseElement.classList.add('error');
        responseElement.style.display = 'block';
    });
}
