
/**
 * TSTO Server Authentication Helper
 * Provides client-side authentication checks and redirects
 */

// Check if user is authenticated by looking for the session cookie
function checkAuthentication() {
    const cookies = document.cookie.split(';');
    let hasSessionCookie = false;
    let sessionToken = '';
    
    console.log('Checking authentication...');
    console.log('Cookies:', document.cookie);
    
    for (let i = 0; i < cookies.length; i++) {
        const cookie = cookies[i].trim();
        if (cookie.startsWith('tsto_session=')) {
            hasSessionCookie = true;
            sessionToken = cookie.substring('tsto_session='.length);
            console.log('Found session token:', sessionToken);
            break;
        }
    }
    
    // If no session cookie found, redirect to login page
    if (!hasSessionCookie || sessionToken === '') {
        console.log('No valid session found, redirecting to login page');
        redirectToLogin();
        return false;
    }
    
    // Skip validation for HTML pages to avoid redirect loops
    const currentPath = window.location.pathname.toLowerCase();
    if (currentPath.endsWith('.html') || currentPath === '/dashboard' || 
        currentPath === '/town_operations' || currentPath === '/game_config') {
        console.log('Dashboard or HTML page detected, skipping validation');
        return true;
    }
    
    // For other pages, validate the session token with the server
    validateSession(sessionToken);
    
    console.log('Authentication check passed');
    return true;
}

// Validate session with the server
function validateSession(token) {
    return fetch('/api/auth/validate_session', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${token}`
        },
        body: JSON.stringify({ token: token }),
        credentials: 'include'
    })
    .then(response => {
        if (!response.ok) {
            console.log('Session validation failed, redirecting to login');
            redirectToLogin();
            return { valid: false };
        }
        return response.json();
    })
    .then(data => {
        if (!data.valid) {
            console.log('Session invalid, redirecting to login');
            redirectToLogin();
        }
        return data;
    })
    .catch(error => {
        console.error('Error validating session:', error);
        // On error, we'll still allow the user to continue
        // The server-side protection will catch invalid sessions
        return { valid: true }; // Assume valid on network errors
    });
}

// Redirect to login page
function redirectToLogin() {
    // Use replace instead of href to prevent back button from returning to protected page
    window.location.replace('/login');
}

// Logout function - clears the session cookie and redirects to login
function logout() {
    console.log('Logging out...');
    
    // First, make a direct call to the server to invalidate the session
    // and only redirect after we get a response or timeout
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
        console.log('Found session token, calling logout endpoint...');
        
        // Create a synchronous XMLHttpRequest to ensure it completes before redirecting
        const xhr = new XMLHttpRequest();
        xhr.open('GET', '/logout', false); // false makes this a synchronous request
        
        try {
            // Send the request and wait for it to complete
            xhr.send();
            console.log('Logout response status:', xhr.status);
        } catch (error) {
            console.error('Error during logout XHR request:', error);
        }
        
        // Clear the session cookie regardless of the response
        document.cookie = 'tsto_session=; expires=Thu, 01 Jan 1970 00:00:00 UTC; path=/;';
        console.log('Session cookie cleared');
    } else {
        console.log('No session token found');
        // Clear the cookie anyway just to be safe
        document.cookie = 'tsto_session=; expires=Thu, 01 Jan 1970 00:00:00 UTC; path=/;';
    }
    
    // Now redirect to the login page
    console.log('Redirecting to login page...');
    window.location.href = '/login.html';
}

// Login function - authenticates with the server and sets session cookie
function login(username, password) {
    return fetch('/api/auth/login', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({ 
            username: username, 
            password: password 
        }),
        credentials: 'include'
    })
    .then(response => {
        if (!response.ok) {
            throw new Error('Login failed');
        }
        return response.json();
    })
    .then(data => {
        if (data.success && data.token) {
            // Set the session cookie
            const expiryDate = new Date();
            expiryDate.setTime(expiryDate.getTime() + (24 * 60 * 60 * 1000)); // 24 hours
            document.cookie = `tsto_session=${data.token}; expires=${expiryDate.toUTCString()}; path=/;`;
            
            // Redirect based on role
            if (data.role === 'ADMIN') {
                window.location.replace('/dashboard.html');
            } else {
                window.location.replace('/town_operations.html');
            }
            return true;
        } else {
            return false;
        }
    });
}

// Run authentication check when the page loads
document.addEventListener('DOMContentLoaded', function() {
    console.log('Page loaded, current path:', window.location.pathname);
    
    // Get the current path
    const currentPath = window.location.pathname.toLowerCase();
    
    // Skip authentication check for login pages
    if (currentPath === '/login' || currentPath === '/login.html') {
        console.log('On login page, skipping authentication check');
        
        // If we're on the login page, add event listener for the login form
        const loginForm = document.getElementById('loginForm');
        if (loginForm) {
            loginForm.addEventListener('submit', function(event) {
                event.preventDefault();
                
                const username = document.getElementById('username').value;
                const password = document.getElementById('password').value;
                const errorMessage = document.getElementById('errorMessage');
                
                if (errorMessage) {
                    errorMessage.style.display = 'none';
                }
                
                login(username, password)
                    .then(success => {
                        if (success) {
                            // Redirect is handled in the login function
                        } else {
                            if (errorMessage) {
                                errorMessage.textContent = 'Invalid username or password';
                                errorMessage.style.display = 'block';
                            }
                        }
                    })
                    .catch(error => {
                        console.error('Login error:', error);
                        if (errorMessage) {
                            errorMessage.textContent = 'Login failed. Please try again.';
                            errorMessage.style.display = 'block';
                        }
                    });
            });
        }
        return;
    }
    
    // Skip authentication check for dashboard pages that are already loaded
    // This prevents redirect loops while maintaining security
    if (currentPath.endsWith('.html') || 
        currentPath === '/dashboard' || 
        currentPath === '/town_operations' || 
        currentPath === '/game_config') {
        console.log('Dashboard page detected, skipping initial authentication check');
        return;
    }
    
    // For API and other endpoints, check authentication
    console.log('Checking authentication for API endpoint or other resource');
    checkAuthentication();
});
