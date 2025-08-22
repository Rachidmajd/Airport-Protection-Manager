// Authentication module with direct login form
class AuthManager {
    constructor() {
        this.token = null;
        this.user = null;
        
        // OAuth endpoints
        this.config = {
            tokenUrl: 'http://192.168.56.105/keycloak/realms/aviation/protocol/openid-connect/token',
            logoutUrl: 'http://192.168.56.105/keycloak/realms/aviation/protocol/openid-connect/logout',
            clientId: 'aeronautical-platform',
            redirectUri: window.location.origin + window.location.pathname
        };
    }

    async init() {
        try {
            console.log('🔄 Initializing authentication...');
            
            // Check if we have a stored token
            const storedToken = localStorage.getItem('aviation_token');
            if (storedToken) {
                try {
                    await this.validateToken(storedToken);
                    console.log('✅ Valid token found, user authenticated');
                    return true;
                } catch (error) {
                    console.log('❌ Stored token invalid, removing...');
                    localStorage.removeItem('aviation_token');
                }
            }
            
            console.log('❌ No valid authentication found');
            return false;
            
        } catch (error) {
            console.error('❌ Failed to initialize authentication:', error);
            throw error;
        }
    }

    async login(username, password) {
        try {
            console.log('🔄 Attempting direct login for:', username);
            
            const response = await fetch(this.config.tokenUrl, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: new URLSearchParams({
                    grant_type: 'password',
                    client_id: this.config.clientId,
                    username: username,
                    password: password,
                    scope: 'openid email profile'
                })
            });
            
            if (response.ok) {
                const tokens = await response.json();
                this.token = tokens.access_token;
                
                // Store token
                localStorage.setItem('aviation_token', this.token);
                
                // Parse user info
                const payload = JSON.parse(atob(this.token.split('.')[1]));
                this.user = {
                    id: payload.sub,
                    username: payload.preferred_username,
                    email: payload.email,
                    firstName: payload.given_name,
                    lastName: payload.family_name,
                    fullName: payload.name,
                    roles: payload.realm_access?.roles || [],
                    permissions: payload.resource_access?.[this.config.clientId]?.roles || []
                };
                
                console.log('✅ Login successful, user:', this.user);
                return { success: true, user: this.user };
                
            } else {
                const errorData = await response.json().catch(() => ({}));
                console.log('❌ Login failed:', errorData);
                
                // Handle different error types
                let errorMessage = 'Login failed';
                if (response.status === 401) {
                    errorMessage = 'Invalid username or password';
                } else if (errorData.error === 'invalid_grant') {
                    errorMessage = 'Invalid username or password';
                } else if (errorData.error_description) {
                    errorMessage = errorData.error_description;
                }
                
                return { success: false, error: errorMessage };
            }
        } catch (error) {
            console.error('❌ Login error:', error);
            return { success: false, error: 'Network error. Please try again.' };
        }
    }

    async validateToken(token) {
        // Simple token validation - check if it's expired
        const payload = JSON.parse(atob(token.split('.')[1]));
        const now = Math.floor(Date.now() / 1000);
        
        if (payload.exp < now) {
            throw new Error('Token expired');
        }
        
        this.token = token;
        this.user = {
            id: payload.sub,
            username: payload.preferred_username,
            email: payload.email,
            firstName: payload.given_name,
            lastName: payload.family_name,
            fullName: payload.name,
            roles: payload.realm_access?.roles || [],
            permissions: payload.resource_access?.[this.config.clientId]?.roles || []
        };
    }

    async logout() {
        try {
            console.log('🔄 Starting logout...');
            
            // Clear stored token
            localStorage.removeItem('aviation_token');
            this.token = null;
            this.user = null;
            
            return true;
        } catch (error) {
            console.error('❌ Logout failed:', error);
            return false;
        }
    }

    getToken() {
        return this.token;
    }

    getUser() {
        return this.user;
    }

    isAuthenticated() {
        return this.token !== null && this.user !== null;
    }

    hasRole(role) {
        if (!this.isAuthenticated()) return false;
        return this.user.roles.includes(role);
    }

    hasPermission(permission) {
        if (!this.isAuthenticated()) return false;
        return this.user.permissions.includes(permission);
    }

    // Get authorization header for API requests
    getAuthHeader() {
        return this.token ? `Bearer ${this.token}` : null;
    }
}

// Create singleton instance
const authManager = new AuthManager();

export default authManager;