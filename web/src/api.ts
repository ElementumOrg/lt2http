// Types and calls of the lt2http API, see Swagger at /swagger/ui for all of it

export interface TorrentStatus {
  total: number;
  total_done: number;
  total_wanted: number;
  total_wanted_done: number;
  progress: number;
  download_rate: number;
  upload_rate: number;
  state: number;
  // The same in every language
  state_string: string;
  state_localized: string;
  connections: number;
  connections_limit: number;
  seeders: number;
  seeders_total: number;
  peers: number;
  peers_total: number;
  seeding_time: number;
  finished_time: number;
  active_time: number;
  total_download: number;
  total_upload: number;
}

export interface TorrentInfo {
  hash: string;
  name: string;
  size: number;
  // Folder of the files on disk, null for memory storage
  download_path: string | null;
  piece_length: number;
  has_metadata: boolean;
  is_memory_storage: boolean;
  memory_size: number | null;
  is_paused: boolean;
  total_files: number;
  chosen_files: number;
  players: number;
  readers: number;
  meta_info: unknown;
  torrent_status: TorrentStatus | null;
}

export interface FileStatus {
  total: number;
  total_done: number;
  progress: number;
  priority: number;
  buffering_total: number;
  buffering_progress: number;
  readers: number;
}

export interface FileInfo {
  id: number;
  size: number;
  offset: number;
  path: string;
  name: string;
  // Absolute link to stream or download the file
  stream: string;
  file_status: FileStatus | null;
}

export type TrackerState = 'working' | 'updating' | 'failed' | 'not_contacted' | 'off';

export interface Tracker {
  url: string;
  // -1 for DHT
  tier: number;
  status: TrackerState;
  fails: number;
  // -1 when unknown
  seeds: number;
  peers: number;
  downloaded: number;
  next_announce: number;
  message: string;
}

export interface Pieces {
  // A character per piece: * stored and read by a stream, ? wanted by a stream, + stored, ~ downloading,
  // 1-7 queued with that priority, - not wanted
  map: string;
  total: number;
  stored: number;
  downloading: number;
  prioritized: number;
  readers: number;
}

export type Settings = Record<string, unknown>;

export interface SettingsState {
  lt2http: Settings;
  libtorrent: Settings;
  session: Settings & { stats: Record<string, number> };
}

export interface Language {
  code: string;
  name: string;
}

export interface Languages {
  default: string;
  languages: Language[];
}

export interface Added {
  hash: string;
  has_metadata: boolean;
}

export type Storage = 'file' | 'memory';

// Errors of the API come as { success: false, error: "..." }
async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(path, init);
  const text = await response.text();
  let data: unknown;
  try {
    data = JSON.parse(text);
  } catch {
    throw new Error(text || response.statusText);
  }
  const error = (data as { success?: boolean; error?: string } | null) ?? {};
  if (!response.ok || error.success === false) throw new Error(error.error || response.statusText);
  return data as T;
}

const torrent = (hash: string) => `/torrents/${encodeURIComponent(hash)}`;

export const api = {
  languages: () => request<Languages>('/languages'),
  settingsState: () => request<SettingsState>('/settings/state'),

  torrents: () => request<TorrentInfo[]>('/torrents?status=true'),
  torrent: (hash: string) => request<TorrentInfo>(`${torrent(hash)}/info?status=true`),
  files: (hash: string, status = true) => request<FileInfo[]>(`${torrent(hash)}/files?status=${status}`),
  trackers: (hash: string) => request<Tracker[]>(`${torrent(hash)}/trackers`),
  pieces: (hash: string) => request<Pieces>(`${torrent(hash)}/pieces`),

  pause: (hash: string) => request(`${torrent(hash)}/pause`),
  resume: (hash: string) => request(`${torrent(hash)}/resume`),
  remove: (hash: string, deleteFiles: boolean, deleteData: boolean) =>
    request(`${torrent(hash)}/remove?deleteFiles=${deleteFiles}&deleteData=${deleteData}`),
  downloadAll: (hash: string) => request(`${torrent(hash)}/files/download`),
  stopAll: (hash: string) => request(`${torrent(hash)}/files/stop`),
  downloadFile: (hash: string, index: number) => request(`${torrent(hash)}/files/${index}/download`),
  stopFile: (hash: string, index: number) => request(`${torrent(hash)}/files/${index}/stop`),
  bufferFile: (hash: string, index: number) => request(`${torrent(hash)}/files/${index}/buffer`),
  setPriority: (hash: string, index: number, priority: number) =>
    request(`${torrent(hash)}/files/${index}/set_priority?priority=${priority}`),

  // A magnet, a link to a .torrent file (headers can follow: url|Key=Value|...) or a path
  addUri: (uri: string, storage: Storage) =>
    request<Added>(`/service/add/uri?storage=${storage}&uri=${encodeURIComponent(uri)}`),
  addFile: (file: File, storage: Storage) => {
    const body = new FormData();
    body.append('file', file);
    return request<Added>(`/service/add/file?storage=${storage}`, { method: 'POST', body });
  },
};
