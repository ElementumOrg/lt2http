import {
  ActionIcon,
  Alert,
  Anchor,
  Button,
  Card,
  Code,
  CopyButton,
  Group,
  NativeSelect,
  Progress,
  SimpleGrid,
  Stack,
  Table,
  Text,
  Title,
} from '@mantine/core';
import { IconCheck, IconCopy } from '@tabler/icons-react';
import { ReactNode, useState } from 'react';
import { api, FileInfo, Tracker } from '../api';
import { bytes, count, duration, percent, speed } from '../format';
import { PiecesMap } from '../components/PiecesMap';
import { RemoveModal } from '../components/RemoveModal';
import { run } from '../components/run';
import { StateBadge } from '../components/StateBadge';
import { navigate, usePolling } from '../hooks';
import { useI18n } from '../i18n';

const PRIORITIES = [0, 1, 2, 3, 4, 5, 6, 7];

function Section({ title, children }: { title: string; children: ReactNode }) {
  return (
    <Card withBorder>
      <Title order={5} mb="sm">
        {title}
      </Title>
      {children}
    </Card>
  );
}

// Label and value rows of a card
function Values({ rows }: { rows: [string, ReactNode][] }) {
  return (
    <Table withRowBorders={false} verticalSpacing={4} horizontalSpacing={0}>
      <Table.Tbody>
        {rows.map(([label, value]) => (
          <Table.Tr key={label}>
            <Table.Td c="dimmed" pr="md" w="45%">
              {label}
            </Table.Td>
            <Table.Td style={{ overflowWrap: 'anywhere' }}>{value}</Table.Td>
          </Table.Tr>
        ))}
      </Table.Tbody>
    </Table>
  );
}

export function TorrentDetails({ hash }: { hash: string }) {
  const { t } = useI18n();
  const { data, error, reload } = usePolling(
    () => Promise.all([api.torrent(hash), api.files(hash), api.trackers(hash), api.pieces(hash)]),
    2000,
    [hash],
  );
  const [removing, setRemoving] = useState(false);

  if (error) return <Alert color="red">{error.message}</Alert>;
  if (!data) return null;
  const [torrent, files, trackers, pieces] = data;
  const s = torrent.torrent_status!;
  const memory = torrent.is_memory_storage;
  const action = (call: () => Promise<unknown>, done?: string) => run(call, done, reload);

  const fileRow = (f: FileInfo) => {
    const fs = f.file_status!;
    return (
      <Table.Tr key={f.id}>
        <Table.Td miw={220}>
          <Text size="sm" style={{ overflowWrap: 'anywhere' }}>
            {f.path}
          </Text>
          <Text size="xs" c="dimmed">
            {t('label.readers')}: {fs.readers}
            {fs.buffering_total > 0 && ` · ${t('label.buffer')} ${percent(fs.buffering_progress)}`}
          </Text>
        </Table.Td>
        <Table.Td>{bytes(f.size)}</Table.Td>
        <Table.Td miw={110}>
          <Progress value={fs.progress * 100} />
          <Text size="xs" ta="center">
            {percent(fs.progress * 100)}
          </Text>
        </Table.Td>
        <Table.Td>
          {memory ? (
            '-'
          ) : (
            <NativeSelect
              size="xs"
              aria-label={t('label.priority')}
              value={String(fs.priority)}
              onChange={(e) => action(() => api.setPriority(hash, f.id, Number(e.currentTarget.value)))}
              data={PRIORITIES.map((p) => ({ value: String(p), label: p === 0 ? t('files.skip') : String(p) }))}
            />
          )}
        </Table.Td>
        <Table.Td>
          <Group gap={4} wrap="nowrap">
            {!memory &&
              (fs.priority > 0 ? (
                <Button size="xs" variant="default" onClick={() => action(() => api.stopFile(hash, f.id))}>
                  {t('action.stop')}
                </Button>
              ) : (
                <Button size="xs" variant="default" onClick={() => action(() => api.downloadFile(hash, f.id))}>
                  {t('action.download')}
                </Button>
              ))}
            <Button
              size="xs"
              variant="default"
              onClick={() => action(() => api.bufferFile(hash, f.id), t('ui.buffering_started'))}
            >
              {t('action.buffer')}
            </Button>
            <CopyButton value={f.stream}>
              {({ copied, copy }) => (
                <Button
                  size="xs"
                  variant={copied ? 'light' : 'default'}
                  color={copied ? 'green' : undefined}
                  onClick={copy}
                >
                  {t(copied ? 'ui.link_copied' : 'ui.copy_link')}
                </Button>
              )}
            </CopyButton>
            <Button size="xs" variant="default" component="a" href={f.stream} target="_blank">
              {t('ui.open')}
            </Button>
            <Button size="xs" variant="default" component="a" href={f.stream} download={f.name}>
              {t('action.download_file')}
            </Button>
          </Group>
        </Table.Td>
      </Table.Tr>
    );
  };

  const trackerRow = (tr: Tracker) => (
    <Table.Tr key={tr.url}>
      <Table.Td style={{ overflowWrap: 'anywhere' }}>{tr.url}</Table.Td>
      <Table.Td>{tr.tier < 0 ? '-' : tr.tier}</Table.Td>
      <Table.Td>{tr.status === 'failed' ? t('tracker.failed', tr.fails) : t(`tracker.${tr.status}`)}</Table.Td>
      <Table.Td>{count(tr.seeds)}</Table.Td>
      <Table.Td>{count(tr.peers)}</Table.Td>
      <Table.Td>{count(tr.downloaded)}</Table.Td>
      <Table.Td>{tr.next_announce < 0 ? '-' : t('tracker.next_announce', tr.next_announce)}</Table.Td>
      <Table.Td>{tr.message}</Table.Td>
    </Table.Tr>
  );

  return (
    <Stack>
      <Group justify="space-between" align="flex-start">
        <Group gap="sm">
          <Title order={3} style={{ overflowWrap: 'anywhere' }}>
            {torrent.name}
          </Title>
          <StateBadge torrent={torrent} />
        </Group>
        <Group gap="xs">
          {!memory && (
            <>
              <Button variant="default" onClick={() => action(() => api.downloadAll(hash))}>
                {t('action.download_all')}
              </Button>
              <Button variant="default" onClick={() => action(() => api.stopAll(hash))}>
                {t('action.stop_all')}
              </Button>
            </>
          )}
          <Button
            variant="default"
            onClick={() => action(() => (torrent.is_paused ? api.resume(hash) : api.pause(hash)))}
          >
            {t(torrent.is_paused ? 'action.resume' : 'action.pause')}
          </Button>
          <Button variant="light" color="red" onClick={() => setRemoving(true)}>
            {t('action.remove')}
          </Button>
          <Button variant="default" component="a" href={`/info?torrent=${hash}`} target="_blank">
            /info
          </Button>
        </Group>
      </Group>

      <SimpleGrid cols={{ base: 1, sm: 2, xl: 4 }}>
        <Section title={t('details.overview')}>
          <Values
            rows={[
              [t('label.infohash'), <Code key="hash">{torrent.hash}</Code>],
              [t('label.storage'), t(memory ? 'storage.memory' : 'storage.file')],
              ...(torrent.download_path
                ? ([
                    [
                      t('label.download_path'),
                      <Group key="path" gap={4} wrap="nowrap" align="flex-start">
                        <Code style={{ overflowWrap: 'anywhere' }}>{torrent.download_path}</Code>
                        <CopyButton value={torrent.download_path}>
                          {({ copied, copy }) => (
                            <ActionIcon
                              variant="subtle"
                              color={copied ? 'green' : 'gray'}
                              onClick={copy}
                              aria-label={t('ui.copy_path')}
                            >
                              {copied ? <IconCheck size={16} /> : <IconCopy size={16} />}
                            </ActionIcon>
                          )}
                        </CopyButton>
                      </Group>,
                    ],
                  ] as [string, ReactNode][])
                : []),
              [t('label.size'), bytes(torrent.size)],
              [t('label.files'), t('torrents.files_chosen', torrent.chosen_files, torrent.total_files)],
              [t('label.pieces_count'), `${pieces.total} × ${bytes(torrent.piece_length)}`],
              [t('label.players'), torrent.players],
              [t('label.readers'), torrent.readers],
              ...(memory ? ([[t('label.memory'), bytes(torrent.memory_size)]] as [string, ReactNode][]) : []),
            ]}
          />
        </Section>
        <Section title={t('section.speed')}>
          <Values
            rows={[
              [t('label.download'), speed(s.download_rate)],
              [t('label.upload'), speed(s.upload_rate)],
              [t('label.progress'), percent(s.progress * 100)],
              [t('label.wanted_done'), `${bytes(s.total_wanted_done)} / ${bytes(s.total_wanted)}`],
              [t('label.total_download'), bytes(s.total_download)],
              [t('label.total_upload'), bytes(s.total_upload)],
              [t('label.active_time'), duration(s.active_time)],
              [t('label.seeding_time'), duration(s.seeding_time)],
            ]}
          />
        </Section>
        <Section title={t('section.connections')}>
          <Values
            rows={[
              [t('label.connections'), `${s.connections} / ${s.connections_limit}`],
              [t('label.connected_seeds'), s.seeders],
              [t('label.connected_peers'), s.peers],
              [t('label.all_seeds'), s.seeders_total],
              [t('label.all_peers'), s.peers_total],
            ]}
          />
        </Section>
        <Section title={t('label.meta_info')}>
          {torrent.meta_info === null || torrent.meta_info === undefined ? (
            <Text size="sm" c="dimmed">
              {t('details.no_meta_info')}
            </Text>
          ) : (
            <Code block>{JSON.stringify(torrent.meta_info, null, 2)}</Code>
          )}
        </Section>
      </SimpleGrid>

      <Section title={`${t('section.files')} (${files.length})`}>
        {files.length ? (
          <Table.ScrollContainer minWidth={900}>
            <Table verticalSpacing="sm">
              <Table.Thead>
                <Table.Tr>
                  <Table.Th>{t('label.name')}</Table.Th>
                  <Table.Th>{t('label.size')}</Table.Th>
                  <Table.Th>{t('label.progress')}</Table.Th>
                  <Table.Th>{t('label.priority')}</Table.Th>
                  <Table.Th />
                </Table.Tr>
              </Table.Thead>
              <Table.Tbody>{files.map(fileRow)}</Table.Tbody>
            </Table>
          </Table.ScrollContainer>
        ) : (
          <Text size="sm" c="dimmed">
            {t('add.waiting_metadata')}
          </Text>
        )}
      </Section>

      <Section title={t('section.trackers')}>
        <Table.ScrollContainer minWidth={900}>
          <Table>
            <Table.Thead>
              <Table.Tr>
                <Table.Th>{t('label.url')}</Table.Th>
                <Table.Th>{t('label.tier')}</Table.Th>
                <Table.Th>{t('label.status')}</Table.Th>
                <Table.Th>{t('label.seeds')}</Table.Th>
                <Table.Th>{t('label.peers')}</Table.Th>
                <Table.Th>{t('label.downloaded')}</Table.Th>
                <Table.Th>{t('label.announce')}</Table.Th>
                <Table.Th>{t('label.message')}</Table.Th>
              </Table.Tr>
            </Table.Thead>
            <Table.Tbody>{trackers.map(trackerRow)}</Table.Tbody>
          </Table>
        </Table.ScrollContainer>
      </Section>

      <Section title={t('section.pieces')}>
        <Text size="sm" mb="sm">
          {t(
            'pieces.summary',
            pieces.total,
            pieces.stored,
            pieces.total ? ((100 * pieces.stored) / pieces.total).toFixed(2) : '0',
            pieces.downloading,
            pieces.prioritized,
            pieces.readers,
          )}
        </Text>
        <PiecesMap map={pieces.map} pieceLength={torrent.piece_length} files={files} />
      </Section>

      <Anchor href="#/" size="sm">
        ← {t('nav.torrents')}
      </Anchor>
      <RemoveModal
        torrent={removing ? torrent : null}
        onClose={() => setRemoving(false)}
        onRemoved={() => navigate('/')}
      />
    </Stack>
  );
}
