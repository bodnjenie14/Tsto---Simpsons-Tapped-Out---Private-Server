// Function to ensure table headers match the data we're displaying
function ensureTableHeadersMatch() {
    // Get the header row
    const headerRow = document.getElementById('results-header-row');
    
    // Make sure it exists
    if (!headerRow) {
        console.error('Header row not found');
        return;
    }
    
    // Clear existing headers
    headerRow.innerHTML = '';
    
    // Define the headers in the correct order to match the data
    const headers = [
        'Email', 'User ID', 'Anon UID', 'AS Identifier', 'Access Token',
        'Device ID', 'Display Name', 'Town Name', 'Land Path',
        'Android ID', 'Vendor ID', 'Client IP', 'Combined ID',
        'Ad ID', 'Platform ID', 'Manufacturer', 'Model',
        'Mayhem ID', 'Access Code', 'User Cred', 'Session Key',
        'Land Token', 'LNGLV Token', 'Actions'
    ];
    
    // Create and append header cells
    headers.forEach(headerText => {
        const th = document.createElement('th');
        th.textContent = headerText;
        headerRow.appendChild(th);
    });
}

document.addEventListener('DOMContentLoaded', function() {
    // Check if user is authenticated and has admin access
    checkAdminAccess();

    // Set up event listeners
    document.getElementById('search-button').addEventListener('click', searchUsers);
    document.getElementById('search-term').addEventListener('keypress', function(e) {
        if (e.key === 'Enter') {
            searchUsers();
        }
    });
    document.getElementById('show-all-button').addEventListener('click', showAllUsers);
    document.getElementById('save-button').addEventListener('click', saveUserChanges);
    document.getElementById('delete-button').addEventListener('click', deleteUser);
    document.getElementById('cancel-button').addEventListener('click', function() {
        document.getElementById('edit-form').style.display = 'none';
    });
    
    // Add event listener for logout button
    const logoutBtn = document.querySelector('.logout-btn');
    if (logoutBtn) {
        logoutBtn.addEventListener('click', function(e) {
            e.preventDefault();
            logoutUser();
        });
    }
    
    // Theme toggle functionality
    const themeToggleButton = document.getElementById('theme-toggle');
    if (themeToggleButton) {
        const themeIcon = themeToggleButton.querySelector('i');
        let isDarkMode = localStorage.getItem('darkMode') === 'true';
        
        // Set initial state
        if (isDarkMode) {
            document.body.classList.add('dark-mode');
            themeIcon.className = 'fas fa-sun';
        } else {
            themeIcon.className = 'fas fa-moon';
        }

        function toggleTheme() {
            isDarkMode = !isDarkMode;
            // Toggle the dark-mode class on the body
            document.body.classList.toggle('dark-mode');
            // Save preference to localStorage
            localStorage.setItem('darkMode', isDarkMode);
            // Update the icon
            if (isDarkMode) {
                themeIcon.className = 'fas fa-sun';
            } else {
                themeIcon.className = 'fas fa-moon';
            }
        }

        themeToggleButton.addEventListener('click', toggleTheme);
    }
});

// Check if user has admin access
function checkAdminAccess() {
    // Get the session token from cookies
    const cookies = document.cookie.split(';');
    let sessionToken = '';
    
    for (let i = 0; i < cookies.length; i++) {
        const cookie = cookies[i].trim();
        if (cookie.startsWith('tsto_session=')) {
            sessionToken = cookie.substring('tsto_session='.length);
            break;
        }
    }
    
    if (sessionToken) {
        // Check if user has admin access
        fetch('/api/auth/validate_session', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ token: sessionToken })
        })
        .then(response => response.json())
        .then(data => {
            if (data.success && data.username) {
                document.getElementById('current-user').innerHTML = `<i class="fas fa-user"></i> Logged in as: ${data.username}`;
            } else {
                // If we're on this page, we must be logged in, so get username from localStorage
                const username = localStorage.getItem('tsto_username') || 'User';
                document.getElementById('current-user').innerHTML = `<i class="fas fa-user"></i> Logged in as: ${username}`;
            }
        })
        .catch(error => {
            console.error('Error validating session:', error);
            // Fallback to showing logged in status without username
            document.getElementById('current-user').innerHTML = `<i class="fas fa-user"></i> Logged in`;
        });
    } else {
        // Fallback - if we're on a protected page, we must be logged in
        document.getElementById('current-user').innerHTML = `<i class="fas fa-user"></i> Logged in`;
    }
}

// Search for users
function searchUsers() {
    // Get search parameters
    const searchField = document.getElementById('search-field').value;
    const searchTerm = document.getElementById('search-term').value.trim();
    
    // Validate search term
    if (!searchTerm) {
        const responseElement = document.getElementById('search-response');
        responseElement.textContent = 'Please enter a search term';
        responseElement.className = 'response error';
        responseElement.style.display = 'block';
        return;
    }
    
    // Show loading spinner
    document.getElementById('loading').style.display = 'block';
    
    // Clear any previous response
    const responseElement = document.getElementById('search-response');
    responseElement.style.display = 'none';
    responseElement.textContent = '';
    responseElement.className = 'response';
    
    // Clear previous results
    document.getElementById('results-body').innerHTML = '';
    document.getElementById('results-table').style.display = 'none';
    
    // Add timestamp to prevent caching
    const timestamp = new Date().getTime();
    
    // Make API call to search users
    console.log(`Searching for users with field: ${searchField}, term: ${searchTerm}`);
    fetch(`/api/search-users?field=${encodeURIComponent(searchField)}&term=${encodeURIComponent(searchTerm)}&_=${timestamp}`, {
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
        document.getElementById('loading').style.display = 'none';
        
        console.log("Search response data:", data);
        
        if (data.success) {
            if (data.users && data.users.length > 0) {
                // Display the results
                displaySearchResults(data.users);
            } else {
                // No users found
                responseElement.textContent = 'No users found matching your search criteria';
                responseElement.classList.add('info');
                responseElement.style.display = 'block';
            }
        } else {
            // Error message
            responseElement.textContent = data.error || 'An error occurred while searching for users';
            responseElement.classList.add('error');
            responseElement.style.display = 'block';
        }
    })
    .catch(error => {
        // Hide loading spinner
        document.getElementById('loading').style.display = 'none';
        
        // Show error message
        console.error('Error searching for users:', error);
        responseElement.textContent = 'An error occurred while searching for users. Please try again.';
        responseElement.classList.add('error');
        responseElement.style.display = 'block';
    });
}

// Display search results in the table
function displaySearchResults(users) {
    const resultsBody = document.getElementById('results-body');
    resultsBody.innerHTML = '';
    
    // First, ensure the table header matches the data we're about to display
    ensureTableHeadersMatch();
    
    users.forEach(user => {
        const row = document.createElement('tr');
        
        // Add email cell
        const emailCell = document.createElement('td');
        emailCell.textContent = user.email || 'N/A';
        row.appendChild(emailCell);
        
        // Add user_id cell
        const userIdCell = document.createElement('td');
        userIdCell.textContent = user.user_id || 'N/A';
        row.appendChild(userIdCell);
        
        // Add anon_uid cell
        const anonUidCell = document.createElement('td');
        anonUidCell.textContent = user.anon_uid || 'N/A';
        row.appendChild(anonUidCell);
        
        // Add as_identifier cell
        const asIdentifierCell = document.createElement('td');
        asIdentifierCell.textContent = user.as_identifier || 'N/A';
        row.appendChild(asIdentifierCell);
        
        // Add access_token cell with truncation
        const accessTokenCell = document.createElement('td');
        if (user.access_token) {
            const truncatedToken = user.access_token.length > 20 
                ? user.access_token.substring(0, 20) + '...' 
                : user.access_token;
            accessTokenCell.textContent = truncatedToken;
            accessTokenCell.title = user.access_token; // Show full token on hover
        } else {
            accessTokenCell.textContent = 'N/A';
        }
        row.appendChild(accessTokenCell);
        
        // Add device_id cell
        const deviceIdCell = document.createElement('td');
        deviceIdCell.textContent = user.device_id || 'N/A';
        row.appendChild(deviceIdCell);
        
        // Add display_name cell
        const displayNameCell = document.createElement('td');
        displayNameCell.textContent = user.display_name || 'N/A';
        row.appendChild(displayNameCell);
        
        // Add town_name cell
        const townNameCell = document.createElement('td');
        townNameCell.textContent = user.town_name || 'N/A';
        row.appendChild(townNameCell);
        
        // Add land_save_path cell
        const landSavePathCell = document.createElement('td');
        landSavePathCell.textContent = user.land_save_path || 'N/A';
        row.appendChild(landSavePathCell);
        
        // Add android_id cell
        const androidIdCell = document.createElement('td');
        androidIdCell.textContent = user.android_id || 'N/A';
        row.appendChild(androidIdCell);
        
        // Add vendor_id cell
        const vendorIdCell = document.createElement('td');
        vendorIdCell.textContent = user.vendor_id || 'N/A';
        row.appendChild(vendorIdCell);
        
        // Add client_ip cell
        const clientIpCell = document.createElement('td');
        clientIpCell.textContent = user.client_ip || 'N/A';
        row.appendChild(clientIpCell);
        
        // Add combined_id cell
        const combinedIdCell = document.createElement('td');
        combinedIdCell.textContent = user.combined_id || 'N/A';
        row.appendChild(combinedIdCell);
        
        // Add advertising_id cell
        const advertisingIdCell = document.createElement('td');
        advertisingIdCell.textContent = user.advertising_id || 'N/A';
        row.appendChild(advertisingIdCell);
        
        // Add platform_id cell
        const platformIdCell = document.createElement('td');
        platformIdCell.textContent = user.platform_id || 'N/A';
        row.appendChild(platformIdCell);
        
        // Add manufacturer cell
        const manufacturerCell = document.createElement('td');
        manufacturerCell.textContent = user.manufacturer || 'N/A';
        row.appendChild(manufacturerCell);
        
        // Add model cell
        const modelCell = document.createElement('td');
        modelCell.textContent = user.model || 'N/A';
        row.appendChild(modelCell);
        
        // Add mayhem_id cell
        const mayhemIdCell = document.createElement('td');
        mayhemIdCell.textContent = user.mayhem_id || 'N/A';
        row.appendChild(mayhemIdCell);
        
        // Add access_code cell
        const accessCodeCell = document.createElement('td');
        accessCodeCell.textContent = user.access_code || 'N/A';
        row.appendChild(accessCodeCell);
        
        // Add user_cred cell
        const userCredCell = document.createElement('td');
        userCredCell.textContent = user.user_cred || 'N/A';
        row.appendChild(userCredCell);
        
        // Add session_key cell with truncation
        const sessionKeyCell = document.createElement('td');
        if (user.session_key) {
            const truncatedKey = user.session_key.length > 20 
                ? user.session_key.substring(0, 20) + '...' 
                : user.session_key;
            sessionKeyCell.textContent = truncatedKey;
            sessionKeyCell.title = user.session_key; // Show full key on hover
        } else {
            sessionKeyCell.textContent = 'N/A';
        }
        row.appendChild(sessionKeyCell);
        
        // Add land_token cell with truncation
        const landTokenCell = document.createElement('td');
        if (user.land_token) {
            const truncatedToken = user.land_token.length > 20 
                ? user.land_token.substring(0, 20) + '...' 
                : user.land_token;
            landTokenCell.textContent = truncatedToken;
            landTokenCell.title = user.land_token; // Show full token on hover
        } else {
            landTokenCell.textContent = 'N/A';
        }
        row.appendChild(landTokenCell);
        
        // Add lnglv_token cell with truncation
        const lnglvTokenCell = document.createElement('td');
        if (user.lnglv_token) {
            const truncatedToken = user.lnglv_token.length > 20 
                ? user.lnglv_token.substring(0, 20) + '...' 
                : user.lnglv_token;
            lnglvTokenCell.textContent = truncatedToken;
            lnglvTokenCell.title = user.lnglv_token; // Show full token on hover
        } else {
            lnglvTokenCell.textContent = 'N/A';
        }
        row.appendChild(lnglvTokenCell);
        
        // Add actions cell
        const actionsCell = document.createElement('td');
        
        // Edit button
        const editButton = document.createElement('button');
        editButton.className = 'action-btn edit-btn';
        editButton.title = 'Edit User';
        editButton.innerHTML = '<i class="fas fa-edit"></i>';
        editButton.onclick = function() {
            editUser(user.email);
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
            confirmDeleteUser(user.email);
        };
        actionsCell.appendChild(deleteButton);
        
        row.appendChild(actionsCell);
        
        resultsBody.appendChild(row);
    });
    
    // Show the results table
    document.getElementById('results-table').style.display = 'table';
}

// Edit user
function editUser(email) {
    console.log("Editing user with email:", email);
    
    // Show loading spinner
    document.getElementById('loading').style.display = 'block';
    
    // Clear any previous response
    const responseElement = document.getElementById('edit-response');
    responseElement.style.display = 'none';
    responseElement.textContent = '';
    responseElement.className = 'response';
    
    // URL encode the email properly
    const encodedEmail = encodeURIComponent(email);
    console.log("Encoded email:", encodedEmail);
    
    // Get user details
    fetch(`/api/get-user?email=${encodedEmail}`, {
        method: 'GET',
        headers: {
            'Content-Type': 'application/json',
            'Cache-Control': 'no-cache'
        },
        credentials: 'include'
    })
    .then(response => {
        console.log("Get user response status:", response.status);
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        // Hide loading spinner
        document.getElementById('loading').style.display = 'none';
        console.log("User data:", data);
        
        if (data.success && data.user) {
            // Fill the edit form with user data
            document.getElementById('edit-email').value = data.user.email || '';
            document.getElementById('edit-user-id').value = data.user.user_id || '';
            document.getElementById('edit-as-identifier').value = data.user.as_identifier || '';
            document.getElementById('edit-access-token').value = data.user.access_token || '';
            document.getElementById('edit-device-id').value = data.user.device_id || '';
            document.getElementById('edit-display-name').value = data.user.display_name || '';
            document.getElementById('edit-town-name').value = data.user.town_name || '';
            document.getElementById('edit-land-save-path').value = data.user.land_save_path || '';
            document.getElementById('edit-android-id').value = data.user.android_id || '';
            document.getElementById('edit-vendor-id').value = data.user.vendor_id || '';
            document.getElementById('edit-client-ip').value = data.user.client_ip || '';
            document.getElementById('edit-combined-id').value = data.user.combined_id || '';
            document.getElementById('edit-advertising-id').value = data.user.advertising_id || '';
            document.getElementById('edit-platform-id').value = data.user.platform_id || '';
            document.getElementById('edit-manufacturer').value = data.user.manufacturer || '';
            document.getElementById('edit-model').value = data.user.model || '';
            document.getElementById('edit-mayhem-id').value = data.user.mayhem_id || '';
            document.getElementById('edit-access-code').value = data.user.access_code || '';
            document.getElementById('edit-user-cred').value = data.user.user_cred || '';
            
            // Set up copy button for access token
            document.getElementById('copy-token-button').addEventListener('click', function() {
                const tokenInput = document.getElementById('edit-access-token');
                tokenInput.select();
                document.execCommand('copy');
                
                // Show a brief "Copied!" tooltip
                const button = this;
                const originalTitle = button.getAttribute('title');
                button.setAttribute('title', 'Copied!');
                setTimeout(function() {
                    button.setAttribute('title', originalTitle);
                }, 1500);
            });
            
            // Show the edit form
            document.getElementById('edit-form').style.display = 'block';
            
            // Scroll to the edit form
            document.getElementById('edit-form').scrollIntoView({ behavior: 'smooth' });
        } else {
            // Show error message
            responseElement.textContent = data.error || 'Failed to get user details';
            responseElement.classList.add('error');
            responseElement.style.display = 'block';
        }
    })
    .catch(error => {
        // Hide loading spinner
        document.getElementById('loading').style.display = 'none';
        
        // Show error message
        console.error('Error getting user details:', error);
        responseElement.textContent = 'An error occurred while getting user details. Please try again.';
        responseElement.classList.add('error');
        responseElement.style.display = 'block';
    });
}

// Save user changes
function saveUserChanges() {
    console.log("Saving user changes...");
    
    // Show loading spinner
    document.getElementById('loading').style.display = 'block';
    
    // Clear any previous response
    const responseElement = document.getElementById('edit-response');
    responseElement.style.display = 'none';
    responseElement.textContent = '';
    responseElement.className = 'response';
    
    // Get the email (primary key)
    const email = document.getElementById('edit-email').value;
    const userData = {
        email: email,
        user_id: document.getElementById('edit-user-id').value,
        as_identifier: document.getElementById('edit-as-identifier').value,
        access_token: document.getElementById('edit-access-token').value,
        device_id: document.getElementById('edit-device-id').value,
        display_name: document.getElementById('edit-display-name').value,
        town_name: document.getElementById('edit-town-name').value,
        land_save_path: document.getElementById('edit-land-save-path').value,
        android_id: document.getElementById('edit-android-id').value,
        vendor_id: document.getElementById('edit-vendor-id').value,
        client_ip: document.getElementById('edit-client-ip').value,
        combined_id: document.getElementById('edit-combined-id').value,
        advertising_id: document.getElementById('edit-advertising-id').value,
        platform_id: document.getElementById('edit-platform-id').value,
        manufacturer: document.getElementById('edit-manufacturer').value,
        model: document.getElementById('edit-model').value,
        mayhem_id: parseInt(document.getElementById('edit-mayhem-id').value) || 0,
        access_code: document.getElementById('edit-access-code').value,
        user_cred: document.getElementById('edit-user-cred').value,
        password: document.getElementById('edit-password').value,
        role: document.getElementById('edit-role').value
    };
    
    // Update user
    fetch('/api/update-user', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(userData),
        credentials: 'include'
    })
    .then(response => {
        console.log("Update user response status:", response.status);
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        // Hide loading spinner
        document.getElementById('loading').style.display = 'none';
        
        if (data.success) {
            // Show success message
            responseElement.textContent = 'User updated successfully';
            responseElement.classList.add('success');
            responseElement.style.display = 'block';
            
            // Refresh the search results
            const searchField = document.getElementById('search-field').value;
            const searchTerm = document.getElementById('search-term').value;
            if (searchTerm) {
                searchUsers();
            }
        } else {
            // Show error message
            responseElement.textContent = data.error || 'Failed to update user';
            responseElement.classList.add('error');
            responseElement.style.display = 'block';
        }
    })
    .catch(error => {
        // Hide loading spinner
        document.getElementById('loading').style.display = 'none';
        
        // Show error message
        console.error('Error updating user:', error);
        responseElement.textContent = 'An error occurred while updating user. Please try again.';
        responseElement.classList.add('error');
        responseElement.style.display = 'block';
    });
}

// Confirm delete user
function confirmDeleteUser(email) {
    if (confirm(`Are you sure you want to delete user ${email}? This action cannot be undone.`)) {
        deleteUser(email);
    }
}

// Delete user
function deleteUser(email) {
    console.log("Deleting user with email:", email);
    
    // Show loading spinner
    document.getElementById('loading').style.display = 'block';
    
    // Clear any previous response
    const responseElement = document.getElementById('search-response');
    responseElement.style.display = 'none';
    responseElement.textContent = '';
    responseElement.className = 'response';
    
    // Delete user - this will first clear sensitive device identification fields before deleting
    fetch(`/api/delete-user?email=${encodeURIComponent(email)}`, {
        method: 'DELETE',
        credentials: 'include'
    })
    .then(response => {
        console.log("Delete user response status:", response.status);
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        // Hide loading spinner
        document.getElementById('loading').style.display = 'none';
        
        if (data.success) {
            // Show success message
            responseElement.textContent = 'User deleted successfully';
            responseElement.classList.add('success');
            responseElement.style.display = 'block';
            
            // Refresh the search results
            const searchField = document.getElementById('search-field').value;
            const searchTerm = document.getElementById('search-term').value;
            if (searchTerm) {
                searchUsers();
            }
        } else {
            // Error message
            responseElement.textContent = data.error || 'Failed to delete user';
            responseElement.classList.add('error');
            responseElement.style.display = 'block';
        }
    })
    .catch(error => {
        // Hide loading spinner
        document.getElementById('loading').style.display = 'none';
        
        // Show error message
        console.error('Error deleting user:', error);
        responseElement.textContent = 'An error occurred while deleting user. Please try again.';
        responseElement.classList.add('error');
        responseElement.style.display = 'block';
    });
}

// Logout user
function logoutUser() {
    try {
        // Call the logout API
        fetch('/api/auth/logout', {
            method: 'GET',
            headers: {
                'Content-Type': 'application/json'
            }
        });
        
        // Clear session token from cookies
        document.cookie = 'tsto_session=; expires=Thu, 01 Jan 1970 00:00:00 UTC; path=/;';
        
        // Clear username from localStorage
        localStorage.removeItem('tsto_username');
        
        // Redirect to login page
        window.location.href = '/login.html';
    } catch (error) {
        console.error('Error logging out:', error);
        // Even if there's an error, try to redirect to login
        window.location.href = '/login.html';
    }
}

// Show all users
function showAllUsers() {
    console.log("Showing all users...");
    
    // Show loading spinner
    document.getElementById('loading').style.display = 'block';
    
    // Clear any previous response
    const responseElement = document.getElementById('search-response');
    responseElement.style.display = 'none';
    responseElement.textContent = '';
    responseElement.className = 'response';
    
    // Clear previous results
    document.getElementById('results-body').innerHTML = '';
    document.getElementById('results-table').style.display = 'none';
    
    // Make API call to get all users
    fetch('/api/get-all-users', {
        method: 'GET',
        credentials: 'include'
    })
    .then(response => {
        console.log("Get all users response status:", response.status);
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        // Hide loading spinner
        document.getElementById('loading').style.display = 'none';
        
        if (data.success) {
            if (data.users && data.users.length > 0) {
                // Display the results
                displaySearchResults(data.users);
            } else {
                // No users found
                responseElement.textContent = 'No users found';
                responseElement.classList.add('info');
                responseElement.style.display = 'block';
            }
        } else {
            // Error message
            responseElement.textContent = data.error || 'An error occurred while getting all users';
            responseElement.classList.add('error');
            responseElement.style.display = 'block';
        }
    })
    .catch(error => {
        // Hide loading spinner
        document.getElementById('loading').style.display = 'none';
        
        // Show error message
        console.error('Error getting all users:', error);
        responseElement.textContent = 'An error occurred while getting all users. Please try again.';
        responseElement.classList.add('error');
        responseElement.style.display = 'block';
    });
}
