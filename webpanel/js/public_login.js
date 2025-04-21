document.addEventListener('DOMContentLoaded', function() {
    // DOM Elements
    const loginSection = document.getElementById('login-section');
    const townSection = document.getElementById('town-section');
    const loginButton = document.getElementById('login-button');
    const loginEmail = document.getElementById('login-email');
    const loginPassword = document.getElementById('login-password');
    const loginResponse = document.getElementById('login-response');
    const logoutButton = document.getElementById('logout-button');
    const exportButton = document.getElementById('export-button');
    const importFile = document.getElementById('import-file');
    const selectedFile = document.getElementById('selected-file');
    const townInfo = document.getElementById('town-info');
    const townResponse = document.getElementById('town-response');

    // Check if user is already logged in
    checkLoginStatus();

    // Event Listeners
    loginButton.addEventListener('click', handleLogin);
    logoutButton.addEventListener('click', handleLogout);
    exportButton.addEventListener('click', exportTown);
    importFile.addEventListener('change', handleFileSelect);

    // Theme toggle
    if (localStorage.getItem('darkMode') === 'true') {
        document.body.classList.add('dark-mode');
        document.querySelector('.theme-toggle i').classList.remove('fa-moon');
        document.querySelector('.theme-toggle i').classList.add('fa-sun');
    }

    // Functions
    function checkLoginStatus() {
        // Get token from localStorage
        const token = localStorage.getItem('publicUserToken');
        const email = localStorage.getItem('publicUserEmail');
        
        if (token && email) {
            // Validate token on server
            fetch('/api/public/validate_token', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    email: email,
                    token: token
                })
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    // Token is valid, show town section
                    showTownSection(data.user_info);
                } else {
                    // Token is invalid, clear localStorage
                    localStorage.removeItem('publicUserToken');
                    localStorage.removeItem('publicUserEmail');
                    showLoginSection();
                }
            })
            .catch(error => {
                console.error('Error validating token:', error);
                showLoginSection();
            });
        } else {
            // No token found, show login section
            showLoginSection();
        }
    }

    function handleLogin() {
        // Get login credentials
        const email = loginEmail.value.trim();
        const password = loginPassword.value;

        // Validate input
        if (!email || !password) {
            showLoginError('Please enter both email and password');
            return;
        }

        // Clear previous response
        loginResponse.textContent = '';
        loginResponse.className = 'response';

        // Show loading message
        loginResponse.textContent = 'Logging in...';
        loginResponse.classList.add('info');

        // Send login request to server
        fetch('/api/public/login', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                email: email,
                password: password
            })
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                // Store token in localStorage
                localStorage.setItem('publicUserToken', data.token);
                localStorage.setItem('publicUserEmail', email);
                
                // Show success message
                loginResponse.textContent = 'Login successful!';
                loginResponse.className = 'response success';
                
                // Show town section
                showTownSection(data.user_info);
            } else {
                // Show error message
                showLoginError(data.error || 'Login failed. Please check your credentials.');
            }
        })
        .catch(error => {
            console.error('Error during login:', error);
            showLoginError('An error occurred. Please try again later.');
        });
    }

    function handleLogout() {
        // Clear localStorage
        localStorage.removeItem('publicUserToken');
        localStorage.removeItem('publicUserEmail');
        
        // Show login section
        showLoginSection();
        
        // Show logout message
        loginResponse.textContent = 'You have been logged out.';
        loginResponse.className = 'response info';
    }

    function showLoginSection() {
        loginSection.classList.add('active');
        townSection.classList.remove('active');
    }

    function showTownSection(userInfo) {
        // Update town info
        if (userInfo) {
            townInfo.innerHTML = `
                <h3>Welcome, ${userInfo.display_name}</h3>
                <p><strong>Email:</strong> ${userInfo.email}</p>
                <p><strong>Registered:</strong> ${new Date(userInfo.registration_date * 1000).toLocaleString()}</p>
                <p><strong>Last Login:</strong> ${new Date(userInfo.last_login * 1000).toLocaleString()}</p>
            `;
        }
        
        // Show town section
        loginSection.classList.remove('active');
        townSection.classList.add('active');
    }

    function showLoginError(message) {
        loginResponse.textContent = message;
        loginResponse.className = 'response error';
    }

    function exportTown() {
        const email = localStorage.getItem('publicUserEmail');
        const token = localStorage.getItem('publicUserToken');
        
        if (!email || !token) {
            showLoginSection();
            return;
        }
        
        // Show loading message
        townResponse.textContent = 'Exporting town...';
        townResponse.className = 'response info';
        
        // Request town export
        fetch(`/api/public/export_town?email=${encodeURIComponent(email)}&token=${encodeURIComponent(token)}`)
        .then(response => {
            if (!response.ok) {
                throw new Error('Export failed');
            }
            return response.blob();
        })
        .then(blob => {
            // Create download link
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.style.display = 'none';
            a.href = url;
            a.download = `${email.split('@')[0]}_town.pb`;
            document.body.appendChild(a);
            a.click();
            window.URL.revokeObjectURL(url);
            
            // Show success message
            townResponse.textContent = 'Town exported successfully!';
            townResponse.className = 'response success';
        })
        .catch(error => {
            console.error('Error exporting town:', error);
            townResponse.textContent = 'Failed to export town. Please try again later.';
            townResponse.className = 'response error';
        });
    }

    function handleFileSelect(event) {
        const file = event.target.files[0];
        if (file) {
            selectedFile.textContent = `Selected file: ${file.name}`;
            
            // Show import confirmation
            townResponse.innerHTML = `
                <p>Ready to import "${file.name}".</p>
                <button id="confirm-import" class="file-input-button">
                    <i class="fas fa-check"></i> Confirm Import
                </button>
            `;
            townResponse.className = 'response info';
            
            // Add event listener to confirm button
            document.getElementById('confirm-import').addEventListener('click', function() {
                importTown(file);
            });
        }
    }

    function importTown(file) {
        const email = localStorage.getItem('publicUserEmail');
        const token = localStorage.getItem('publicUserToken');
        
        if (!email || !token) {
            showLoginSection();
            return;
        }
        
        // Create form data
        const formData = new FormData();
        formData.append('file', file);
        formData.append('email', email);
        formData.append('token', token);
        
        // Show loading message
        townResponse.textContent = 'Importing town...';
        townResponse.className = 'response info';
        
        // Send import request
        fetch('/api/public/import_town', {
            method: 'POST',
            body: formData
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                townResponse.textContent = 'Town imported successfully!';
                townResponse.className = 'response success';
            } else {
                townResponse.textContent = data.error || 'Failed to import town.';
                townResponse.className = 'response error';
            }
        })
        .catch(error => {
            console.error('Error importing town:', error);
            townResponse.textContent = 'An error occurred during import. Please try again later.';
            townResponse.className = 'response error';
        });
    }
});

// Theme toggle function
function toggleTheme() {
    const body = document.body;
    const icon = document.querySelector('.theme-toggle i');
    
    body.classList.toggle('dark-mode');
    
    if (body.classList.contains('dark-mode')) {
        icon.classList.remove('fa-moon');
        icon.classList.add('fa-sun');
        localStorage.setItem('darkMode', 'true');
    } else {
        icon.classList.remove('fa-sun');
        icon.classList.add('fa-moon');
        localStorage.setItem('darkMode', 'false');
    }
}
