// =============================================================================
// FUMBBL OAuth2 Authentication Service
// Handles authentication flow with the FUMBBL server
// =============================================================================

import {
  OAuthTokenResponse,
  SessionTokenResponse,
  FumbblApiConfig,
  DEFAULT_API_CONFIG,
} from '../types/fumbblApiTypes';

// -----------------------------------------------------------------------------
// FUMBBL Authentication Service
// -----------------------------------------------------------------------------

export class FumbblAuthService {
  private config: FumbblApiConfig;
  private accessToken: string | null = null;
  private sessionToken: string | null = null;
  private tokenExpiresAt: number | null = null;
  // Username from OAuth2 identity (stored here for use in WebSocket join)
  _username: string = '';

  constructor(config?: FumbblApiConfig) {
    this.config = { ...DEFAULT_API_CONFIG, ...config };
  }

  // ---------------------------------------------------------------------------
  // OAuth2 client_credentials Flow
  // ---------------------------------------------------------------------------

  /**
   * Exchange client credentials for an access token
   * POST https://fumbbl.com/api/oauth/token
   */
  async authenticate(clientId: string, clientSecret: string): Promise<OAuthTokenResponse> {
    const url = `${this.config.baseUrl.replace('/api', '')}${'/api/oauth/token'}`;

    const formData = new URLSearchParams();
    formData.append('grant_type', 'client_credentials');
    formData.append('client_id', clientId);
    formData.append('client_secret', clientSecret);

    const response = await fetch(url, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded',
      },
      body: formData.toString(),
    });

    if (!response.ok) {
      const errorText = await response.text();
      throw new Error(`OAuth authentication failed: ${response.status} - ${errorText}`);
    }

    const data: OAuthTokenResponse = await response.json();
    this.accessToken = data.access_token;
    // Token expires in `expires_in` seconds from now
    this.tokenExpiresAt = Date.now() + data.expires_in * 1000;

    return data;
  }

  /**
   * Fetch a session token from the server using the access token
   * POST https://fumbbl.com/api/auth/getToken
   */
  async fetchSessionToken(): Promise<SessionTokenResponse> {
    if (!this.accessToken) {
      throw new Error('Not authenticated. Call authenticate() first.');
    }

    const url = `${this.config.baseUrl.replace('/api', '')}${'/api/auth/getToken'}`;
    console.log('[FumbblAuth] Fetching session token from:', url);

    const response = await fetch(url, {
      method: 'POST',
      headers: {
        'Accept': 'application/json',
        'Authorization': `Bearer ${this.accessToken}`,
      },
    });

    if (!response.ok) {
      const errorText = await response.text();
      console.error('[FumbblAuth] Session token request failed:', response.status, errorText);
      throw new Error(`Session token request failed: ${response.status} - ${errorText}`);
    }

    const data: SessionTokenResponse = await response.json();
    console.log('[FumbblAuth] Session token response:', JSON.stringify(data));
    console.log('[FumbblAuth] session_token value:', data.session_token);
    console.log('[FumbblAuth] All response keys:', Object.keys(data));

    // The API might return sessionToken instead of session_token
    this.sessionToken = data.session_token || (data as any).sessionToken || null;
    console.log('[FumbblAuth] Final sessionToken:', this.sessionToken);

    return data;
  }

  /**
   * Complete the full authentication flow:
   * 1. Get access token via OAuth2 client_credentials
   * 2. Get session token via /api/auth/getToken
   */
  async fullAuthFlow(clientId: string, clientSecret: string): Promise<{
    accessToken: string;
    sessionToken: string;
    expiresIn: number;
  }> {
    const oauthResponse = await this.authenticate(clientId, clientSecret);
    await this.fetchSessionToken();

    console.log('[FumbblAuth] fullAuthFlow returning:', {
      accessToken: this.accessToken?.substring(0, 20) + '...',
      sessionToken: this.sessionToken ? this.sessionToken.substring(0, 20) + '...' : 'undefined',
      expiresIn: oauthResponse.expires_in,
    });

    return {
      accessToken: this.accessToken!,
      sessionToken: this.sessionToken!,
      expiresIn: oauthResponse.expires_in,
    };
  }

  // ---------------------------------------------------------------------------
  // Token Management
  // ---------------------------------------------------------------------------

  /** Set the access token manually (e.g., from stored credentials) */
  setAccessToken(token: string): void {
    this.accessToken = token;
    // Assume token is valid for 1 hour if not from authenticate()
    this.tokenExpiresAt = Date.now() + 3600000;
  }

  /** Set the session token manually (e.g., from stored credentials) */
  setSessionToken(token: string): void {
    this.sessionToken = token;
  }

  /** Get the current access token */
  getAccessToken(): string | null {
    return this.accessToken;
  }

  /** Get the current session token string */
  getSessionToken(): string | null {
    return this.sessionToken;
  }

  /** Get the username from OAuth2 identity */
  getUsername(): string {
    return this._username;
  }

  /** Check if the access token is still valid */
  isTokenValid(): boolean {
    if (!this.accessToken || !this.tokenExpiresAt) {
      return false;
    }
    // Consider token valid until 1 minute before expiration
    return Date.now() < (this.tokenExpiresAt - 60000);
  }

  /** Check if we need to refresh the token */
  needsRefresh(): boolean {
    return !this.isTokenValid();
  }

  /** Clear all tokens */
  clearTokens(): void {
    this.accessToken = null;
    this.sessionToken = null;
    this.tokenExpiresAt = null;
  }

  // ---------------------------------------------------------------------------
  // OAuth Identity Verification
  // ---------------------------------------------------------------------------

  /**
   * Verify OAuth identity
   * GET https://fumbbl.com/api/oauth/identity
   */
  async getIdentity(): Promise<{ client_id: string; scope: string; user_id?: number; user_name?: string }> {
    if (!this.accessToken) {
      throw new Error('Not authenticated. Call authenticate() first.');
    }

    const url = `${this.config.baseUrl.replace('/api', '')}${'/api/oauth/identity'}`;

    const response = await fetch(url, {
      method: 'GET',
      headers: {
        'Authorization': `Bearer ${this.accessToken}`,
      },
    });

    if (!response.ok) {
      throw new Error(`Identity verification failed: ${response.status}`);
    }

    return await response.json();
  }

  // ---------------------------------------------------------------------------
  // Session Token Verification
  // ---------------------------------------------------------------------------

  /**
   * Verify a session token
   * POST https://fumbbl.com/api/auth/verify
   */
  async verifySessionToken(sessionToken: string): Promise<boolean> {
    const url = `${this.config.baseUrl.replace('/api', '')}${'/api/auth/verify'}`;

    const response = await fetch(url, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ token: sessionToken }),
    });

    return response.ok;
  }

  // ---------------------------------------------------------------------------
  // Authorization Header Helper
  // ---------------------------------------------------------------------------

  /** Get the Authorization header value */
  getAuthHeader(): string | null {
    if (!this.accessToken) {
      return null;
    }
    return `Bearer ${this.accessToken}`;
  }

  /** Get headers with authentication included */
  getAuthHeaders(): Record<string, string> {
    const headers: Record<string, string> = {
      'Accept': 'application/json',
      'Content-Type': 'application/json',
    };

    const authHeader = this.getAuthHeader();
    if (authHeader) {
      headers['Authorization'] = authHeader;
    }

    return headers;
  }
}

// -----------------------------------------------------------------------------
// Singleton instance for use across the app
// -----------------------------------------------------------------------------

export const fumbblAuth = new FumbblAuthService();