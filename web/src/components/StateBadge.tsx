import { Badge } from '@mantine/core';
import { TorrentInfo } from '../api';
import { useI18n } from '../i18n';

const COLORS: Record<string, string> = {
  Downloading: 'blue',
  Buffering: 'blue',
  Playing: 'teal',
  Seeding: 'green',
  Finished: 'green',
  Stalled: 'orange',
  Checking: 'orange',
  Finding: 'orange',
  Error: 'red',
};

export function StateBadge({ torrent }: { torrent: TorrentInfo }) {
  const { t } = useI18n();
  const state = torrent.torrent_status?.state_string ?? 'Queued';
  return (
    <Badge color={COLORS[state] ?? 'gray'} variant="light" style={{ flexShrink: 0 }}>
      {t(`state.${state}`)}
    </Badge>
  );
}
