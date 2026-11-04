import {
  ActionIcon,
  Alert,
  Anchor,
  Button,
  Card,
  Group,
  Progress,
  Stack,
  Table,
  Text,
  Title,
  Tooltip,
} from '@mantine/core';
import { IconPlayerPause, IconPlayerPlay, IconPlus, IconTrash } from '@tabler/icons-react';
import { useState } from 'react';
import { api, TorrentInfo } from '../api';
import { bytes, percent, speed } from '../format';
import { RemoveModal } from '../components/RemoveModal';
import { run } from '../components/run';
import { StateBadge } from '../components/StateBadge';
import { usePolling } from '../hooks';
import { useI18n } from '../i18n';

export function TorrentList() {
  const { t } = useI18n();
  const { data, error, reload } = usePolling(api.torrents);
  const [removing, setRemoving] = useState<TorrentInfo | null>(null);

  if (error) return <Alert color="red">{error.message}</Alert>;
  if (!data) return null;

  if (!data.length) {
    return (
      <Stack align="center" mt="xl">
        <Text c="dimmed">{t('torrents.empty')}</Text>
        <Button component="a" href="#/add" leftSection={<IconPlus size={16} />}>
          {t('nav.add')}
        </Button>
      </Stack>
    );
  }

  const torrents = [...data].sort((a, b) => a.name.localeCompare(b.name));
  const total = (rate: 'download_rate' | 'upload_rate') =>
    torrents.reduce((sum, x) => sum + (x.torrent_status?.[rate] ?? 0), 0);

  const rows = torrents.map((torrent) => {
    const s = torrent.torrent_status!;
    return (
      <Table.Tr key={torrent.hash}>
        <Table.Td miw={220}>
          <Anchor href={`#/torrent/${torrent.hash}`} style={{ overflowWrap: 'anywhere' }}>
            {torrent.name}
          </Anchor>
          <Text size="xs" c="dimmed">
            {t(torrent.is_memory_storage ? 'storage.memory' : 'storage.file')} ·{' '}
            {t('torrents.files_chosen', torrent.chosen_files, torrent.total_files)}
          </Text>
        </Table.Td>
        <Table.Td>{bytes(torrent.size)}</Table.Td>
        <Table.Td miw={120}>
          <Progress value={s.progress * 100} size="lg" />
          <Text size="xs" ta="center">
            {percent(s.progress * 100)}
          </Text>
        </Table.Td>
        <Table.Td>
          <StateBadge torrent={torrent} />
        </Table.Td>
        <Table.Td>{speed(s.download_rate)}</Table.Td>
        <Table.Td>{speed(s.upload_rate)}</Table.Td>
        <Table.Td>
          {s.seeders}/{s.seeders_total} · {s.peers}/{s.peers_total}
        </Table.Td>
        <Table.Td>
          <Group gap={4} wrap="nowrap">
            <Tooltip label={t(torrent.is_paused ? 'action.resume' : 'action.pause')}>
              <ActionIcon
                variant="default"
                aria-label={t(torrent.is_paused ? 'action.resume' : 'action.pause')}
                onClick={() =>
                  run(() => (torrent.is_paused ? api.resume(torrent.hash) : api.pause(torrent.hash)), undefined, reload)
                }
              >
                {torrent.is_paused ? <IconPlayerPlay size={16} /> : <IconPlayerPause size={16} />}
              </ActionIcon>
            </Tooltip>
            <Tooltip label={t('action.remove')}>
              <ActionIcon
                variant="default"
                color="red"
                aria-label={t('action.remove')}
                onClick={() => setRemoving(torrent)}
              >
                <IconTrash size={16} />
              </ActionIcon>
            </Tooltip>
          </Group>
        </Table.Td>
      </Table.Tr>
    );
  });

  return (
    <Stack>
      <Group justify="space-between">
        <Title order={3}>
          {t('nav.torrents')} ({torrents.length})
        </Title>
        <Text c="dimmed">
          ↓ {speed(total('download_rate'))} · ↑ {speed(total('upload_rate'))}
        </Text>
      </Group>
      <Card withBorder p={0}>
        <Table.ScrollContainer minWidth={900}>
          <Table highlightOnHover verticalSpacing="sm">
            <Table.Thead>
              <Table.Tr>
                <Table.Th>{t('label.name')}</Table.Th>
                <Table.Th>{t('label.size')}</Table.Th>
                <Table.Th>{t('label.progress')}</Table.Th>
                <Table.Th>{t('label.status')}</Table.Th>
                <Table.Th>{t('label.download')}</Table.Th>
                <Table.Th>{t('label.upload')}</Table.Th>
                <Table.Th>{t('label.seeds_peers')}</Table.Th>
                <Table.Th />
              </Table.Tr>
            </Table.Thead>
            <Table.Tbody>{rows}</Table.Tbody>
          </Table>
        </Table.ScrollContainer>
      </Card>
      <RemoveModal torrent={removing} onClose={() => setRemoving(null)} onRemoved={reload} />
    </Stack>
  );
}
