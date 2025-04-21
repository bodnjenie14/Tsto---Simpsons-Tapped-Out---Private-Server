let selectedEventTime = 0;
let editor;
let currentSearchPos = null;

async function loadUsers() {
    try {
        const response = await fetch('/list_users');
        if (!response.ok) {
            throw new Error('Failed to fetch users');
        }
        const data = await response.json();
        const userSelect = document.getElementById('userSelect');
        const userSelectSave = document.getElementById('userSelectSave');

        // Clear existing options except the first one
        while (userSelect.options.length > 1) {
            userSelect.remove(1);
        }

        while (userSelectSave.options.length > 1) {
            userSelectSave.remove(1);
        }

        // Add users to dropdown
        data.users.forEach(user => {
            const option = document.createElement('option');
            option.value = JSON.stringify(user);
            option.textContent = user.isLegacy ? 'Legacy User (mytown)' : user.username;
            userSelect.appendChild(option);
            userSelectSave.appendChild(option.cloneNode(true));
        });
    } catch (error) {
        console.error('Error loading users:', error);
    }
}

function userSelected() {
    const userSelect = document.getElementById('userSelect');
    const userEmail = document.getElementById('userEmail');
    const donutAmount = document.getElementById('donutAmount');

    if (userSelect.value) {
        const userData = JSON.parse(userSelect.value);
        userEmail.value = userData.username;
        donutAmount.value = userData.currency;
    } else {
        userEmail.value = '';
        donutAmount.value = '';
    }
}

async function userSelectedSave() {
    const userSelectSave = document.getElementById('userSelectSave');

    if (!userSelectSave || !userSelectSave.value) return;
    
    try {
        const userData = JSON.parse(userSelectSave.value);
        
        // Clear any previous save data
        if (editor) {
            editor.setValue('');
        }
        
        // Show loading message
        const saveArea = document.getElementById('saveArea');
        if (saveArea) {
            saveArea.innerText = 'Loading save data...';
        }
        
        // Load the user's save data
        await loadUserSave();
    } catch (error) {
        console.error('Error in userSelectedSave:', error);
    }
}

function updateSelectedEvent() {
    const eventSelect = document.getElementById('eventSelect');
    if (eventSelect && eventSelect.value) {
        selectedEventTime = parseInt(eventSelect.value);
        console.log('Selected event time updated to:', selectedEventTime);
    }
}

function setSelectedEvent() {
    if (!selectedEventTime) {
        updateSelectedEvent(); // Make sure we have a selected event time
    }
    
    if (confirm('Are you sure you want to change the current event?')) {
        console.log('Setting event time:', selectedEventTime);
        
        fetch('/api/events/set', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                event_time: selectedEventTime
            })
        })
        .then(response => {
            console.log('Response status:', response.status);
            return response.json();
        })
        .then(data => {
            console.log('Response data:', data);
            if (data.error) {
                alert('Error: ' + data.error);
            } else {
                alert('Event successfully changed to: ' + (data.current_event || 'Unknown'));
                location.reload();
            }
        })
        .catch(error => {
            console.error('Error:', error);
            alert('Error setting event: ' + error);
        });
    }
}

function adjustEventTime(minutesOffset) {
    if (confirm(`Are you sure you want to adjust the event time by ${minutesOffset > 0 ? '+' : ''}${minutesOffset} minutes?`)) {
        console.log(`Adjusting event time by ${minutesOffset} minutes`);

        fetch('/api/events/adjust_time', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                minutes_offset: minutesOffset
            })
        })
            .then(response => {
                console.log('Response status:', response.status);
                return response.json();
            })
            .then(data => {
                console.log('Response data:', data);
                if (data.error) {
                    alert('Error: ' + data.error);
                } else {
                    alert(`Event time successfully adjusted by ${minutesOffset > 0 ? '+' : ''}${minutesOffset} minutes.`);
                    location.reload();
                }
            })
            .catch(error => {
                console.error('Error:', error);
                alert('Error adjusting event time: ' + error);
            });
    }
}

function resetEventTime() {
    if (confirm('Are you sure you want to reset the event time to current time?')) {
        console.log('Resetting event time to current time');

        fetch('/api/events/reset_time', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        })
            .then(response => {
                console.log('Response status:', response.status);
                return response.json();
            })
            .then(data => {
                console.log('Response data:', data);
                if (data.error) {
                    alert('Error: ' + data.error);
                } else {
                    alert('Event time successfully reset to current time.');
                    location.reload();
                }
            })
            .catch(error => {
                console.error('Error:', error);
                alert('Error resetting event time: ' + error);
            });
    }
}

function restartServer() {
    if (confirm('Are you sure you want to restart the server?')) {
        fetch('/api/server/restart', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        })
            .then(response => response.json())
            .then(data => alert(data.message))
            .catch(error => alert('Error: ' + error));
    }
}

function stopServer() {
    if (confirm('Are you sure you want to stop the server?')) {
        fetch('/api/server/stop', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        })
            .then(response => response.json())
            .then(data => alert(data.message))
            .catch(error => alert('Error: ' + error));
    }
}

async function updateInitialDonuts() {
    const initialDonuts = document.getElementById('initialDonuts').value;
    try {
        const response = await fetch('/api/update_initial_donuts', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ initialDonuts: parseInt(initialDonuts) })
        });

        if (!response.ok) {
            throw new Error('Failed to update initial donuts');
        }

        alert('Initial donuts updated successfully!');
    } catch (error) {
        alert('Error updating initial donuts: ' + error.message);
    }
}

async function updateCurrentDonuts() {
    const currentDonuts = document.getElementById('currentDonuts').value;
    try {
        const response = await fetch('/update_current_donuts', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ currentDonuts: parseInt(currentDonuts) })
        });

        if (!response.ok) {
            throw new Error('Failed to update current donuts');
        }

        alert('Current donuts updated successfully!');
    } catch (error) {
        alert('Error updating current donuts: ' + error.message);
    }
}

async function browseDlcDirectory() {
    try {
        const response = await fetch('/api/browseDirectory', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        });
        const data = await response.json();
        if (data.success) {
            document.getElementById('dlcDirectory').value = data.path;
            // Automatically update when folder is selected
            updateDlcDirectory();
        } else {
            console.error('Failed to select directory:', data.error);
        }
    } catch (err) {
        console.error('Error selecting directory:', err);
    }
}

async function updateDlcDirectory() {
    const dlcDirectory = document.getElementById('dlcDirectory').value;
    try {
        const response = await fetch('/api/updateDlcDirectory', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ dlcDirectory })
        });
        const data = await response.json();
        if (data.success) {
            alert('DLC directory updated successfully. Please restart the server for changes to take effect.');
        } else {
            alert('Failed to update DLC directory: ' + (data.error || 'Unknown error'));
        }
    } catch (error) {
        alert('Failed to update DLC directory: ' + error);
    }
}

async function updateServerIp() {
    const serverIp = document.getElementById('serverIp').value;
    try {
        const response = await fetch('/api/updateServerIp', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ serverIp })
        });
        const data = await response.json();
        if (data.success) {
            alert('Server IP updated successfully');
        } else {
            alert('Failed to update server IP: ' + (data.error || 'Unknown error'));
        }
    } catch (error) {
        alert('Failed to update server IP: ' + error);
    }
}

async function updateServerPort() {
    const serverPort = document.getElementById('serverPort').value;
    try {
        const response = await fetch('/api/updateServerPort', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ serverPort: parseInt(serverPort) })
        });
        const data = await response.json();
        if (data.success) {
            alert(data.message || 'Server port updated successfully');
        } else {
            alert('Failed to update server port: ' + (data.error || 'Unknown error'));
        }
    } catch (error) {
        alert('Failed to update server port: ' + error);
    }
}

async function forceSaveProtoland() {
    try {
        const response = await fetch('/api/forceSaveProtoland', {
            method: 'POST'
        });

        if (!response.ok) {
            throw new Error('Failed to save town');
        }

        alert('Town saved successfully!');
    } catch (error) {
        alert('Error saving town: ' + error.message);
    }
}

async function updateUserCurrency() {
    const email = document.getElementById('userEmail').value;
    const amount = parseInt(document.getElementById('donutAmount').value);

    if (!email || !amount) {
        alert('Please fill in both email and amount fields');
        return;
    }

    try {
        const response = await fetch('/edit_user_currency', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                email: email,
                amount: amount
            })
        });

        const data = await response.json();
        if (data.status === 'success') {
            alert(`Successfully updated currency for ${email} to ${amount} donuts`);
            // Refresh the user list to show updated currency
            await loadUsers();
        } else {
            alert(`Error: ${data.message}`);
        }
    } catch (error) {
        alert('Failed to update currency: ' + error.message);
    }
}

async function loadUserSave() {
    const userSelectSave = document.getElementById('userSelectSave');

    if (!userSelectSave || !userSelectSave.value) {
        console.error('No user selected');
        return;
    }

    try {
        const user = JSON.parse(userSelectSave.value);
        const currentValue = userSelectSave.value; // Store current selection

        // Show loading state
        const editorContainer = document.getElementById('editor-container');
        if (!editorContainer) {
            console.error('Editor container not found');
            return;
        }
        
        editorContainer.innerHTML = '<div class="loading">Loading save data...</div>';

        const response = await fetch('/api/get-user-save', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                username: user.username,
                isLegacy: user.isLegacy || user.username === 'mytown'
            })
        });

        const data = await response.json();

        if (!response.ok || !data.status || data.status !== 'success') {
            throw new Error(data.error || 'Failed to load save data');
        }

        // Restore selection
        userSelectSave.value = currentValue;

        // Clear loading state
        editorContainer.innerHTML = '';

        // Create a basic textarea first for instant display
        const textarea = document.createElement('textarea');
        textarea.value = data.save;
        textarea.style.display = 'none';
        editorContainer.appendChild(textarea);

        // Initialize CodeMirror with minimal features
        editor = CodeMirror(editorContainer, {
            value: data.save,
            mode: {name: "javascript", json: true},
            lineNumbers: true,
            theme: document.getElementById('darkModeToggle')?.checked ? 'simpsons-dark' : 'default',
            lineWrapping: true,
            viewportMargin: 10,
            indentUnit: 2,
            smartIndent: true,
            tabSize: 2,
            scrollbarStyle: "native",
            matchBrackets: true,
            gutters: ["CodeMirror-linenumbers"],
            firstLineNumber: 1
        });

        // Set editor styles
        const cmElement = editorContainer.querySelector('.CodeMirror');
        if (cmElement) {
            cmElement.style.height = '500px';
            cmElement.style.border = '2px solid var(--simpsons-blue)';
            cmElement.style.borderRadius = '10px';
            cmElement.style.fontSize = '14px';
            cmElement.style.fontFamily = 'monospace';
        }

        console.log('User save loaded successfully');

    } catch (error) {
        console.error('Error loading user save:', error);
        const editorContainer = document.getElementById('editor-container');
        if (editorContainer) {
            editorContainer.innerHTML = `<div class="error">Error loading save: ${error.message}</div>`;
        }
    }
}

function searchInSave() {
    const query = document.getElementById('saveSearchInput').value;
    if (!editor || !query) return;

    currentSearchPos = null;
    const cursor = editor.getSearchCursor(query);
    if (cursor.findNext()) {
        currentSearchPos = cursor.from();
        editor.setSelection(cursor.from(), cursor.to());
        editor.scrollIntoView(cursor.from(), 20);
    }
}

function findNext() {
    const query = document.getElementById('saveSearchInput').value;
    if (!editor || !query) return;

    let found = false;
    const cursor = editor.getSearchCursor(query);

    // Try to find from current position
    while (cursor.findNext()) {
        if (cursor.from().line > editor.getCursor().line ||
            (cursor.from().line === editor.getCursor().line &&
                cursor.from().ch > editor.getCursor().ch)) {
            found = true;
            break;
        }
    }

    // If not found after current position, start from beginning
    if (!found) {
        cursor.from = { line: 0, ch: 0 };
        cursor.findNext();
    }

    if (cursor.from()) {
        editor.setSelection(cursor.from(), cursor.to());
        editor.scrollIntoView(cursor.from(), 20);
    }
}

function findPrev() {
    const query = document.getElementById('saveSearchInput').value;
    if (!editor || !query) return;

    // Store current cursor position
    const startPos = editor.getCursor('from');

    // Try to find previous occurrence
    const cursor = editor.getSearchCursor(query, startPos);
    if (cursor.findPrevious()) {
        editor.setSelection(cursor.from(), cursor.to());
        editor.scrollIntoView(cursor.from(), 20);
        return;
    }

    // If not found, wrap to end and search backwards
    const lastLine = editor.lastLine();
    const endPos = { line: lastLine, ch: editor.getLine(lastLine).length };
    const wrapCursor = editor.getSearchCursor(query, endPos);

    if (wrapCursor.findPrevious()) {
        editor.setSelection(wrapCursor.from(), wrapCursor.to());
        editor.scrollIntoView(wrapCursor.from(), 20);
    }
}

// Toggle advanced mode
function toggleAdvancedMode() {
    const isAdvanced = document.getElementById('advancedModeToggle').checked;
    document.body.classList.toggle('advanced-mode', isAdvanced);
    localStorage.setItem('advancedMode', isAdvanced.toString());

    // Toggle visibility of advanced mode elements
    const advancedElements = document.querySelectorAll('.advanced-mode-only, .advanced-panel');
    advancedElements.forEach(element => {
        element.style.display = isAdvanced ? 'block' : 'none';
    });

    // Special handling for flex containers
    const advancedFlexElements = document.querySelectorAll('.advanced-mode-only-flex');
    advancedFlexElements.forEach(element => {
        element.style.display = isAdvanced ? 'flex' : 'none';
    });

    // Special handling for grid containers
    const advancedGridElements = document.querySelectorAll('.advanced-mode-only-grid');
    advancedGridElements.forEach(element => {
        element.style.display = isAdvanced ? 'grid' : 'none';
    });
}

// Toggle dark mode
function toggleDarkMode() {
    const darkModeEnabled = document.getElementById('darkModeToggle').checked;

    document.documentElement.setAttribute('data-theme', darkModeEnabled ? 'dark' : 'light');

    // Update editor theme immediately
    if (editor) {
        editor.setOption('theme', darkModeEnabled ? 'simpsons-dark' : 'default');
    }

    // Save preference
    localStorage.setItem('darkMode', darkModeEnabled);
}

// Load user preferences from localStorage
function loadUserPreferences() {
    // Load advanced mode preference
    const advancedMode = localStorage.getItem('advancedMode') === 'true';
    document.getElementById('advancedModeToggle').checked = advancedMode;
    if (advancedMode) {
        document.body.classList.add('advanced-mode');

        // Show advanced mode elements
        const advancedElements = document.querySelectorAll('.advanced-mode-only, .advanced-panel');
        advancedElements.forEach(element => {
            element.style.display = 'block';
        });

        // Show flex containers
        const advancedFlexElements = document.querySelectorAll('.advanced-mode-only-flex');
        advancedFlexElements.forEach(element => {
            element.style.display = 'flex';
        });

        // Show grid containers
        const advancedGridElements = document.querySelectorAll('.advanced-mode-only-grid');
        advancedGridElements.forEach(element => {
            element.style.display = 'grid';
        });
    } else {
        // Hide advanced mode elements
        const advancedElements = document.querySelectorAll('.advanced-mode-only, .advanced-panel');
        advancedElements.forEach(element => {
            element.style.display = 'none';
        });

        // Hide flex containers
        const advancedFlexElements = document.querySelectorAll('.advanced-mode-only-flex');
        advancedFlexElements.forEach(element => {
            element.style.display = 'none';
        });

        // Hide grid containers
        const advancedGridElements = document.querySelectorAll('.advanced-mode-only-grid');
        advancedGridElements.forEach(element => {
            element.style.display = 'none';
        });
    }

    // Load dark mode preference
    const darkMode = localStorage.getItem('darkMode') === 'true';
    document.getElementById('darkModeToggle').checked = darkMode;
    if (darkMode) {
        document.documentElement.setAttribute('data-theme', 'dark');
    } else {
        document.documentElement.removeAttribute('data-theme');
    }
}

// Load users when page loads
document.addEventListener('DOMContentLoaded', function () {
    loadUserPreferences();
    
    // Load data in parallel using Promise.all
    Promise.all([
        loadDashboardData(),
        loadUsers()
    ])
    .then(() => {
        console.log('All data loaded successfully');
    })
    .catch(error => {
        console.error('Error loading data:', error);
        // Show error message
        const errorMessage = document.createElement('div');
        errorMessage.className = 'error-message';
        errorMessage.innerHTML = '<i class="fas fa-exclamation-triangle"></i> Failed to load dashboard data. Please refresh the page.';
        document.querySelector('.container')?.prepend(errorMessage);
    });
        
    // Set up event listeners for form elements
    setupEventListeners();
    
    // Set up auto-refresh for active players count
    setInterval(refreshActivePlayers, 30000); // Every 30 seconds
    
    // Set up auto-refresh for server uptime
    startUptimeRefresh();
});

// Add event listeners for page visibility and navigation events
document.addEventListener('visibilitychange', function () {
    if (document.visibilityState === 'visible') {
        // Page is now visible (user returned to this tab)
        loadUsers();
        refreshActivePlayers();
        checkAndFixPlaceholders();
    }
});

// Use the Performance Navigation API to detect when page is loaded from cache (back/forward navigation)
window.addEventListener('pageshow', function (event) {
    // If the page is loaded from cache (back/forward navigation)
    if (event.persisted) {
        loadUsers();
        refreshActivePlayers();
        checkAndFixPlaceholders();
    }
});

// Use History API to detect navigation
window.addEventListener('popstate', function () {
    // This fires when back/forward buttons are used
    loadUsers();
    refreshActivePlayers();
    checkAndFixPlaceholders();
});

// Function to load dashboard data from API
async function loadDashboardData() {
    try {
        const response = await fetch('/api/dashboard/data', {
            headers: {
                'Cache-Control': 'no-cache',
                'Pragma': 'no-cache'
            }
        });
        
        if (!response.ok) {
            throw new Error('Failed to fetch dashboard data');
        }
        
        const data = await response.json();
        console.log('Dashboard data loaded:', data);
        
        // Server settings
        if (data.server_ip) {
            document.getElementById('serverIp').value = data.server_ip;
        }
        if (data.game_port) {
            document.getElementById('serverPort').value = data.game_port;
        }
        if (data.dlc_directory) {
            document.getElementById('dlcDirectory').value = data.dlc_directory;
        }
        
        // Backup settings
        if (data.backup_directory) {
            document.getElementById('backupDirectory').value = data.backup_directory;
        }
        if (data.backup_interval_hours !== undefined) {
            document.getElementById('backupIntervalHours').value = data.backup_interval_hours;
        }
        if (data.backup_interval_seconds !== undefined) {
            document.getElementById('backupIntervalSeconds').value = data.backup_interval_seconds;
        }
        
        // API settings
        if (data.api_enabled !== undefined) {
            document.getElementById('apiEnabled').value = data.api_enabled ? 'true' : 'false';
        }
        if (data.api_key) {
            document.getElementById('apiKey').value = data.api_key;
        }
        if (data.team_name) {
            document.getElementById('teamName').value = data.team_name;
        }
        if (data.require_code !== undefined) {
            document.getElementById('requireCode').value = data.require_code ? 'true' : 'false';
        }
        
        // Land settings
        if (data.use_legacy_mode !== undefined) {
            document.getElementById('useLegacyMode').value = data.use_legacy_mode ? 'true' : 'false';
        }
        
        // Security settings
        if (data.disable_anonymous_users !== undefined) {
            document.getElementById('disableAnonymousUsers').value = data.disable_anonymous_users ? 'true' : 'false';
        }
        
        // Update active players count
        if (data.unique_clients !== undefined) {
            document.getElementById('active-players').innerText = data.unique_clients;
        }
        
        // Update event select dropdown
        if (data.events) {
            const eventSelect = document.getElementById('eventSelect');
            
            // Store current selection
            const currentSelection = eventSelect.value;
            
            // Clear existing options except the first one (Normal Play)
            while (eventSelect.options.length > 1) {
                eventSelect.remove(1);
            }
            
            // Add event options
            for (const [time, name] of Object.entries(data.events)) {
                if (time === '0') continue; // Skip Normal Play as it's already added
                
                const option = document.createElement('option');
                option.value = time;
                option.textContent = name;
                
                // Select current event
                if (parseInt(time) === data.current_event_time) {
                    option.selected = true;
                    selectedEventTime = parseInt(time);
                }
                
                eventSelect.appendChild(option);
            }
            
            // If we had a previous selection and it wasn't selected by the current_event_time logic,
            // try to restore it
            if (currentSelection && !Array.from(eventSelect.options).some(opt => opt.selected && opt.value !== '0')) {
                const matchingOption = Array.from(eventSelect.options).find(opt => opt.value === currentSelection);
                if (matchingOption) {
                    matchingOption.selected = true;
                    selectedEventTime = parseInt(currentSelection);
                }
            }
        }
        
        console.log('Dashboard data loaded successfully');
    } catch (error) {
        console.error('Error loading dashboard data:', error);
    }
}

// Function to check and fix placeholders that weren't replaced by the server
function checkAndFixPlaceholders() {
    const uptimeDisplay = document.getElementById('uptimeDisplay');
    const serverIpInput = document.getElementById('serverIp');
    const serverPortInput = document.getElementById('serverPort');
    const dlcDirectoryInput = document.getElementById('dlcDirectory');
    const currentEventDisplay = document.getElementById('currentEventDisplay');
    
    // Check and fix uptime
    if (uptimeDisplay && uptimeDisplay.innerText.includes('%UPTIME%')) {
        uptimeDisplay.innerText = 'Unknown';
        console.warn('Server did not replace %UPTIME% placeholder');
    }
    
    // Check and fix server IP
    if (serverIpInput && serverIpInput.value.includes('%SERVER_IP%')) {
        serverIpInput.value = '127.0.0.1';
        console.warn('Server did not replace %SERVER_IP% placeholder');
    }
    
    // Check and fix server port
    if (serverPortInput && serverPortInput.value.includes('%GAME_PORT%')) {
        serverPortInput.value = '9090';
        console.warn('Server did not replace %GAME_PORT% placeholder');
    }
    
    // Check and fix DLC directory
    if (dlcDirectoryInput && dlcDirectoryInput.value.includes('%DLC_DIRECTORY%')) {
        dlcDirectoryInput.value = 'dlc';
        console.warn('Server did not replace %DLC_DIRECTORY% placeholder');
    }
    
    // Check and fix current event
    if (currentEventDisplay && currentEventDisplay.innerText.includes('%CURRENT_EVENT%')) {
        currentEventDisplay.innerText = 'Normal Play';
        console.warn('Server did not replace %CURRENT_EVENT% placeholder');
    }
    
    // Check if event rows weren't replaced
    const eventSelect = document.getElementById('eventSelect');
    if (eventSelect && eventSelect.innerHTML.includes('%EVENT_ROWS%')) {
        // Remove the placeholder
        eventSelect.innerHTML = eventSelect.innerHTML.replace('%EVENT_ROWS%', '');
        console.warn('Server did not replace %EVENT_ROWS% placeholder');
        
        // Load dashboard data which includes events
        refreshActivePlayers();
    }
}

// Function to refresh active players count
async function refreshActivePlayers() {
    try {
        const response = await fetch('/api/dashboard/data', {
            headers: {
                'Cache-Control': 'no-cache',
                'Pragma': 'no-cache'
            }
        });

        if (!response.ok) {
            throw new Error('Failed to fetch dashboard data');
        }

        const data = await response.json();
        console.log('Dashboard data:', data);

        // Update active players count - show unique clients
        const activePlayersElement = document.getElementById('active-players');
        if (data.unique_clients !== undefined) {
            activePlayersElement.innerText = data.unique_clients;
        } else if (data.active_connections !== undefined) {
            activePlayersElement.innerText = data.active_connections;
        } else {
            activePlayersElement.innerText = 'Unknown';
        }

        // Also update current event display
        if (data.current_event) {
            document.getElementById('currentEventDisplay').innerText = data.current_event;
        }

        // Update event select dropdown if events data is available
        if (data.events) {
            const eventSelect = document.getElementById('eventSelect');
            
            // Store current selection
            const currentSelection = eventSelect.value;

            // Clear existing options except the first one (Normal Play)
            while (eventSelect.options.length > 1) {
                eventSelect.remove(1);
            }

            // Add event options
            for (const [time, name] of Object.entries(data.events)) {
                if (time === '0') continue; // Skip Normal Play as it's already added

                const option = document.createElement('option');
                option.value = time;
                option.textContent = name;

                // Select current event
                if (data.current_event_time && parseInt(time) === data.current_event_time) {
                    option.selected = true;
                    selectedEventTime = parseInt(time);
                }

                eventSelect.appendChild(option);
            }

            // If we had a previous selection and it wasn't selected by the current_event_time logic,
            // try to restore it
            if (currentSelection && !Array.from(eventSelect.options).some(opt => opt.selected && opt.value !== '0')) {
                const matchingOption = Array.from(eventSelect.options).find(opt => opt.value === currentSelection);
                if (matchingOption) {
                    matchingOption.selected = true;
                    selectedEventTime = parseInt(currentSelection);
                }
            }
        }
    } catch (error) {
        console.error('Error refreshing active players:', error);
    }
}

// Function to refresh server uptime
async function refreshServerUptime() {
    try {
        const response = await fetch('/api/server/uptime');
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        
        const data = await response.json();
        
        // Get the uptime display element
        const uptimeDisplay = document.getElementById('uptimeDisplay');
        
        if (uptimeDisplay) {
            // Format the uptime into a readable string
            const uptime = parseInt(data.uptime);
            const days = Math.floor(uptime / 86400);
            const hours = Math.floor((uptime % 86400) / 3600);
            const minutes = Math.floor((uptime % 3600) / 60);
            const seconds = uptime % 60;
            
            let uptimeString = '';
            if (days > 0) {
                uptimeString += `${days}d `;
            }
            uptimeString += `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
            
            uptimeDisplay.textContent = uptimeString;
            console.log('Server uptime updated:', uptimeString);
        } else {
            console.error('Uptime display element not found');
        }
    } catch (error) {
        console.error('Error refreshing server uptime:', error);
        const uptimeDisplay = document.getElementById('uptimeDisplay');
        if (uptimeDisplay) {
            uptimeDisplay.textContent = 'Error fetching uptime';
        }
    }
}

// Set up interval to refresh uptime every 5 seconds
let uptimeRefreshInterval;

function startUptimeRefresh() {
    // Clear any existing interval
    if (uptimeRefreshInterval) {
        clearInterval(uptimeRefreshInterval);
    }
    
    // Refresh immediately
    refreshServerUptime();
    
    // Set up interval for future refreshes
    uptimeRefreshInterval = setInterval(refreshServerUptime, 5000);
}

// Call this function when the page loads
document.addEventListener('DOMContentLoaded', function() {
    startUptimeRefresh();
    
    // Also refresh active players count
    refreshActivePlayers();
    
    // Load dashboard data
    loadDashboardData();
});

async function browseBackupDirectory() {
    try {
        const response = await fetch('/api/browseDirectory', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        });
        const data = await response.json();
        if (data.success) {
            document.getElementById('backupDirectory').value = data.path;
            console.log('Selected backup directory:', data.path);
            // Automatically update when folder is selected
            updateBackupSettings();
        } else {
            console.error('Failed to select directory:', data.error);
            alert('Failed to select directory: ' + (data.error || 'Unknown error'));
        }
    } catch (err) {
        console.error('Error selecting directory:', err);
        alert('Error selecting directory: ' + err);
    }
}

async function updateBackupSettings() {
    const backupDirectory = document.getElementById('backupDirectory').value;
    const backupIntervalHours = parseInt(document.getElementById('backupIntervalHours').value) || 0;
    const backupIntervalSeconds = parseInt(document.getElementById('backupIntervalSeconds').value) || 0;
    
    console.log('Updating backup settings:', {
        backupDirectory,
        backupIntervalHours,
        backupIntervalSeconds
    });
    
    try {
        // First update the backup directory
        const dirResponse = await fetch('/api/update_dlc_directory', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ 
                directory: backupDirectory
            })
        });
        
        const dirData = await dirResponse.json();
        if (!dirData.success) {
            alert('Failed to update backup directory: ' + (dirData.error || 'Unknown error'));
            return;
        }
        
        // Then update the server configuration
        const serverResponse = await fetch('/api/dashboard/data', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ 
                backup_directory: backupDirectory, 
                backup_interval_hours: backupIntervalHours, 
                backup_interval_seconds: backupIntervalSeconds 
            })
        });
        
        const serverData = await serverResponse.json();
        if (serverData.success) {
            alert('Backup settings updated successfully');
            // Refresh the dashboard data
            loadDashboardData();
        } else {
            alert('Failed to update backup settings: ' + (serverData.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error updating backup settings:', error);
        alert('Failed to update backup settings: ' + error);
    }
}

async function updateApiSettings() {
    const apiEnabled = document.getElementById('apiEnabled').value === 'true';
    const apiKey = document.getElementById('apiKey').value;
    const teamName = document.getElementById('teamName').value;
    const requireCode = document.getElementById('requireCode').value === 'true';
    
    console.log('Updating API settings:', {
        apiEnabled,
        apiKey: apiKey ? '(set)' : '(not set)',
        teamName,
        requireCode
    });
    
    try {
        // Update the server configuration directly
        const response = await fetch('/api/dashboard/data', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ 
                api_enabled: apiEnabled, 
                api_key: apiKey, 
                team_name: teamName, 
                require_code: requireCode 
            })
        });
        
        const data = await response.json();
        if (data.success) {
            alert('API settings updated successfully');
            // Refresh the dashboard data
            loadDashboardData();
        } else {
            alert('Failed to update API settings: ' + (data.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error updating API settings:', error);
        alert('Failed to update API settings: ' + error);
    }
}

async function updateLandSettings() {
    const useLegacyMode = document.getElementById('useLegacyMode').value === 'true';
    
    console.log('Updating land settings:', {
        useLegacyMode
    });
    
    try {
        // Update the server configuration directly
        const response = await fetch('/api/dashboard/data', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ 
                use_legacy_mode: useLegacyMode 
            })
        });
        
        const data = await response.json();
        if (data.success) {
            alert('Land settings updated successfully');
            // Refresh the dashboard data
            loadDashboardData();
        } else {
            alert('Failed to update land settings: ' + (data.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error updating land settings:', error);
        alert('Failed to update land settings: ' + error);
    }
}

async function updateSecuritySettings() {
    const disableAnonymousUsers = document.getElementById('disableAnonymousUsers').value === 'true';
    
    console.log('Updating security settings:', {
        disableAnonymousUsers
    });
    
    try {
        // Update the server configuration directly
        const response = await fetch('/api/dashboard/data', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ 
                disable_anonymous_users: disableAnonymousUsers 
            })
        });
        
        const data = await response.json();
        if (data.success) {
            alert('Security settings updated successfully');
            // Refresh the dashboard data
            loadDashboardData();
        } else {
            alert('Failed to update security settings: ' + (data.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error updating security settings:', error);
        alert('Failed to update security settings: ' + error);
    }
}

// Function to load dashboard configuration
async function loadDashboardConfig() {
    try {
        const response = await fetch('/api/dashboard/config', {
            headers: {
                'Cache-Control': 'no-cache',
                'Pragma': 'no-cache'
            }
        });
        
        if (!response.ok) {
            throw new Error('Failed to fetch dashboard configuration');
        }
        
        const data = await response.json();
        console.log('Dashboard config loaded:', data);
        
        if (data.success && data.config) {
            const config = data.config;
            
            // Backup settings
            if (config.backup) {
                document.getElementById('backupDirectory').value = config.backup.backupDirectory || '';
                document.getElementById('backupIntervalHours').value = config.backup.backupIntervalHours || 12;
                document.getElementById('backupIntervalSeconds').value = config.backup.backupIntervalSeconds || 0;
            }
            
            // API settings
            if (config.api) {
                document.getElementById('apiEnabled').value = config.api.apiEnabled ? 'true' : 'false';
                document.getElementById('apiKey').value = config.api.apiKey || '';
                document.getElementById('teamName').value = config.api.teamName || 'Bodnjenie';
                document.getElementById('requireCode').value = config.api.requireCode ? 'true' : 'false';
            }
            
            // Land settings
            if (config.land) {
                document.getElementById('useLegacyMode').value = config.land.useLegacyMode ? 'true' : 'false';
            }
            
            // Security settings
            if (config.security) {
                document.getElementById('disableAnonymousUsers').value = config.security.disableAnonymousUsers ? 'true' : 'false';
            }
            
            // Server settings
            if (data.game_port) {
                document.getElementById('serverPort').value = data.game_port;
            }
            
            console.log('Dashboard configuration loaded successfully');
        } else {
            console.error('Failed to load dashboard configuration:', data.error || 'Unknown error');
        }
        
        // Also load server data from the dashboard data endpoint
        await loadServerData();
    } catch (error) {
        console.error('Error loading dashboard configuration:', error);
    }
}

// Function to load server data
async function loadServerData() {
    try {
        const response = await fetch('/api/dashboard/data', {
            headers: {
                'Cache-Control': 'no-cache',
                'Pragma': 'no-cache'
            }
        });
        
        if (!response.ok) {
            throw new Error('Failed to fetch dashboard data');
        }
        
        const data = await response.json();
        console.log('Dashboard data loaded:', data);
        
        // Server settings
        if (data.server_ip) {
            document.getElementById('serverIp').value = data.server_ip;
        }
        if (data.game_port) {
            document.getElementById('serverPort').value = data.game_port;
        }
        if (data.dlc_directory) {
            document.getElementById('dlcDirectory').value = data.dlc_directory;
        }
        
        console.log('Server data loaded successfully');
    } catch (error) {
        console.error('Error loading server data:', error);
    }
}

async function updateServerPort() {
    const serverPort = parseInt(document.getElementById('serverPort').value) || 8080;
    
    console.log('Updating server port:', serverPort);
    
    try {
        const response = await fetch('/api/updateServerPort', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ 
                serverPort: serverPort 
            })
        });
        
        const data = await response.json();
        if (data.success) {
            alert(data.message || 'Server port updated successfully');
        } else {
            alert('Failed to update server port: ' + (data.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error updating server port:', error);
        alert('Failed to update server port: ' + error);
    }
}

// Function to update backup settings
async function updateBackupSettings() {
    const backupDirectory = document.getElementById('backupDirectory').value;
    const backupIntervalHours = parseInt(document.getElementById('backupIntervalHours').value) || 0;
    const backupIntervalSeconds = parseInt(document.getElementById('backupIntervalSeconds').value) || 0;
    
    console.log('Updating backup settings:', {
        backupDirectory,
        backupIntervalHours,
        backupIntervalSeconds
    });
    
    try {
        const response = await fetch('/api/dashboard/config', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ 
                backupDirectory, 
                backupIntervalHours, 
                backupIntervalSeconds 
            })
        });
        
        const data = await response.json();
        if (data.success) {
            alert('Backup settings updated successfully');
            // Refresh the dashboard configuration
            loadDashboardConfig();
        } else {
            alert('Failed to update backup settings: ' + (data.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error updating backup settings:', error);
        alert('Failed to update backup settings: ' + error);
    }
}

// Function to update API settings
async function updateApiSettings() {
    const apiEnabled = document.getElementById('apiEnabled').value === 'true';
    const apiKey = document.getElementById('apiKey').value;
    const teamName = document.getElementById('teamName').value;
    const requireCode = document.getElementById('requireCode').value === 'true';
    
    console.log('Updating API settings:', {
        apiEnabled,
        apiKey: apiKey ? '(set)' : '(not set)',
        teamName,
        requireCode
    });
    
    try {
        const response = await fetch('/api/dashboard/config', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ 
                apiEnabled, 
                apiKey, 
                teamName, 
                requireCode 
            })
        });
        
        const data = await response.json();
        if (data.success) {
            alert('API settings updated successfully');
            // Refresh the dashboard configuration
            loadDashboardConfig();
        } else {
            alert('Failed to update API settings: ' + (data.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error updating API settings:', error);
        alert('Failed to update API settings: ' + error);
    }
}

// Function to update land settings
async function updateLandSettings() {
    const useLegacyMode = document.getElementById('useLegacyMode').value === 'true';
    
    console.log('Updating land settings:', {
        useLegacyMode
    });
    
    try {
        const response = await fetch('/api/dashboard/config', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ 
                useLegacyMode 
            })
        });
        
        const data = await response.json();
        if (data.success) {
            alert('Land settings updated successfully');
            // Refresh the dashboard configuration
            loadDashboardConfig();
        } else {
            alert('Failed to update land settings: ' + (data.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error updating land settings:', error);
        alert('Failed to update land settings: ' + error);
    }
}

// Function to update security settings
async function updateSecuritySettings() {
    const disableAnonymousUsers = document.getElementById('disableAnonymousUsers').value === 'true';
    
    console.log('Updating security settings:', {
        disableAnonymousUsers
    });
    
    try {
        const response = await fetch('/api/dashboard/config', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ 
                disableAnonymousUsers 
            })
        });
        
        const data = await response.json();
        if (data.success) {
            alert('Security settings updated successfully');
            // Refresh the dashboard configuration
            loadDashboardConfig();
        } else {
            alert('Failed to update security settings: ' + (data.error || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error updating security settings:', error);
        alert('Failed to update security settings: ' + error);
    }
}

// Function to browse backup directory
async function browseBackupDirectory() {
    try {
        const response = await fetch('/api/browse_directory', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ 
                currentPath: document.getElementById('backupDirectory').value || '' 
            })
        });
        
        const data = await response.json();
        if (data.success && data.selectedPath) {
            document.getElementById('backupDirectory').value = data.selectedPath;
        }
    } catch (error) {
        console.error('Error browsing directory:', error);
        alert('Failed to browse directory: ' + error);
    }
}

// Load dashboard configuration when page loads
document.addEventListener('DOMContentLoaded', function() {
    loadDashboardConfig();
    // Other initialization code...
});

// Function to set up all event listeners for the dashboard
function setupEventListeners() {
    // User selection and currency management
    const userSelect = document.getElementById('userSelect');
    if (userSelect) {
        userSelect.addEventListener('change', userSelected);
    }
    
    const userSelectSave = document.getElementById('userSelectSave');
    if (userSelectSave) {
        userSelectSave.addEventListener('change', userSelectedSave);
    }
    
    // Button event listeners
    const updateCurrencyBtn = document.getElementById('updateCurrencyBtn');
    if (updateCurrencyBtn) {
        updateCurrencyBtn.addEventListener('click', updateUserCurrency);
    }
    
    // Event selection
    const eventSelect = document.getElementById('eventSelect');
    if (eventSelect) {
        eventSelect.addEventListener('change', updateSelectedEvent);
    }
    
    // Toggle switches
    const darkModeToggle = document.getElementById('darkModeToggle');
    if (darkModeToggle) {
        darkModeToggle.addEventListener('change', toggleDarkMode);
    }
    
    const advancedModeToggle = document.getElementById('advancedModeToggle');
    if (advancedModeToggle) {
        advancedModeToggle.addEventListener('change', toggleAdvancedMode);
    }
    
    // Search functionality
    const searchBtn = document.getElementById('searchBtn');
    if (searchBtn) {
        searchBtn.addEventListener('click', searchInSave);
    }
    
    const nextBtn = document.getElementById('nextBtn');
    if (nextBtn) {
        nextBtn.addEventListener('click', findNext);
    }
    
    const prevBtn = document.getElementById('prevBtn');
    if (prevBtn) {
        prevBtn.addEventListener('click', findPrev);
    }
    
    // Save editor
    const saveBtn = document.getElementById('saveBtn');
    if (saveBtn) {
        saveBtn.addEventListener('click', loadUserSave);
    }
    
    // Server control buttons
    const restartServerBtn = document.getElementById('restartServerBtn');
    if (restartServerBtn) {
        restartServerBtn.addEventListener('click', restartServer);
    }
    
    const stopServerBtn = document.getElementById('stopServerBtn');
    if (stopServerBtn) {
        stopServerBtn.addEventListener('click', stopServer);
    }
    
    // Advanced configuration buttons
    // Backup settings
    const browseBackupBtn = document.querySelector('button[onclick="browseBackupDirectory()"]');
    if (browseBackupBtn) {
        browseBackupBtn.removeAttribute('onclick');
        browseBackupBtn.addEventListener('click', browseBackupDirectory);
    }
    
    const updateBackupBtn = document.querySelectorAll('button[onclick="updateBackupSettings()"]');
    updateBackupBtn.forEach(btn => {
        if (btn) {
            btn.removeAttribute('onclick');
            btn.addEventListener('click', updateBackupSettings);
        }
    });
    
    // API settings
    const updateApiBtn = document.querySelectorAll('button[onclick="updateApiSettings()"]');
    updateApiBtn.forEach(btn => {
        if (btn) {
            btn.removeAttribute('onclick');
            btn.addEventListener('click', updateApiSettings);
        }
    });
    
    // Land settings
    const updateLandBtn = document.querySelectorAll('button[onclick="updateLandSettings()"]');
    updateLandBtn.forEach(btn => {
        if (btn) {
            btn.removeAttribute('onclick');
            btn.addEventListener('click', updateLandSettings);
        }
    });
    
    // Security settings
    const updateSecurityBtn = document.querySelectorAll('button[onclick="updateSecuritySettings()"]');
    updateSecurityBtn.forEach(btn => {
        if (btn) {
            btn.removeAttribute('onclick');
            btn.addEventListener('click', updateSecuritySettings);
        }
    });
    
    // Event time adjustment buttons
    const eventTimeButtons = document.querySelectorAll('.time-controls-grid button');
    eventTimeButtons.forEach(btn => {
        if (btn.classList.contains('reset-button')) {
            btn.removeAttribute('onclick');
            btn.addEventListener('click', resetEventTime);
        } else {
            const minutesOffset = parseInt(btn.getAttribute('onclick').match(/adjustEventTime\((-?\d+)\)/)[1]);
            btn.removeAttribute('onclick');
            btn.addEventListener('click', () => adjustEventTime(minutesOffset));
        }
    });
    
    // Add event listeners for Enter key in search input
    const searchInput = document.getElementById('saveSearchInput');
    if (searchInput) {
        searchInput.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                searchInSave();
            }
        });
    }
    
    console.log('Event listeners set up successfully');
}