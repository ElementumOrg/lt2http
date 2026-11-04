import {
  Alert,
  Button,
  Card,
  Checkbox,
  Code,
  FileInput,
  Group,
  Loader,
  SegmentedControl,
  Stack,
  Table,
  Text,
  TextInput,
  Title,
} from '@mantine/core';
import { notifications } from '@mantine/notifications';
import { FormEvent, useEffect, useState } from 'react';
import { api, FileInfo, Storage } from '../api';
import { bytes } from '../format';
import { run } from '../components/run';
import { navigate, usePolling } from '../hooks';
import { useI18n } from '../i18n';

export function AddTorrent() {
  const { t } = useI18n();
  const [uri, setUri] = useState('');
  const [file, setFile] = useState<File | null>(null);
  const [storage, setStorage] = useState<Storage>('file');
  const [onlyAdd, setOnlyAdd] = useState(false);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string>();
  const [added, setAdded] = useState<string>();

  // The storage of the download_storage setting is the default
  useEffect(() => {
    api
      .settingsState()
      .then((state) => state.lt2http.download_storage === 'memory' && setStorage('memory'))
      .catch(() => undefined);
  }, []);

  const submit = async (event: FormEvent) => {
    event.preventDefault();
    setError(undefined);
    if (!file && !uri.trim()) return setError(t('add.nothing'));
    setBusy(true);
    try {
      const result = file ? await api.addFile(file, storage) : await api.addUri(uri.trim(), storage);
      // Files are not downloaded until chosen, memory storage downloads what is played
      if (onlyAdd || storage === 'memory') {
        notifications.show({ message: t(onlyAdd ? 'add.added' : 'add.memory_hint'), color: 'green' });
        navigate(`/torrent/${result.hash}`);
      } else {
        setAdded(result.hash);
      }
    } catch (e) {
      setError((e as Error).message);
    } finally {
      setBusy(false);
    }
  };

  if (added) return <ChooseFiles hash={added} />;

  return (
    <Stack maw={800}>
      <Title order={3}>{t('nav.add')}</Title>
      <Card withBorder component="form" onSubmit={submit}>
        <Stack>
          <TextInput
            label={t('add.link')}
            placeholder="magnet:?xt=urn:btih:... / https://.../file.torrent"
            value={uri}
            onChange={(e) => setUri(e.currentTarget.value)}
            description={
              <>
                {t('add.link_hint')} <Code>https://host/file.torrent|Cookie=a=1|User-Agent=Kodi</Code>
              </>
            }
          />
          <FileInput
            label={t('add.file')}
            accept=".torrent,application/x-bittorrent"
            value={file}
            onChange={setFile}
            clearable
          />
          <Stack gap={4}>
            <Text size="sm" fw={500}>
              {t('label.storage')}
            </Text>
            <SegmentedControl
              value={storage}
              onChange={(value) => setStorage(value as Storage)}
              data={[
                { value: 'file', label: t('storage.file') },
                { value: 'memory', label: t('storage.memory') },
              ]}
            />
          </Stack>
          <Checkbox label={t('add.only_add')} checked={onlyAdd} onChange={(e) => setOnlyAdd(e.currentTarget.checked)} />
          {error && <Alert color="red">{error}</Alert>}
          <Group justify="flex-end">
            <Button type="submit" loading={busy}>
              {t('nav.add')}
            </Button>
          </Group>
        </Stack>
      </Card>
    </Stack>
  );
}

// Waits for the metadata of the added torrent, then shows its files to choose
function ChooseFiles({ hash }: { hash: string }) {
  const { t } = useI18n();
  const { data: files, error } = usePolling<FileInfo[]>(() => api.files(hash, false), 1000, [hash]);
  const [skipped, setSkipped] = useState<Set<number>>(new Set());
  const [busy, setBusy] = useState(false);

  if (error) return <Alert color="red">{error.message}</Alert>;
  const skip = (
    <Button variant="default" component="a" href={`#/torrent/${hash}`}>
      {t('add.skip')}
    </Button>
  );
  if (!files?.length) {
    return (
      <Card withBorder maw={800}>
        <Group>
          <Loader size="sm" />
          <Text>{t('add.waiting_metadata')}</Text>
        </Group>
        <Group justify="flex-end" mt="md">
          {skip}
        </Group>
      </Card>
    );
  }

  const chosen = files.filter((f) => !skipped.has(f.id));
  const toggle = (id: number) =>
    setSkipped((current) => {
      const next = new Set(current);
      if (next.has(id)) next.delete(id);
      else next.add(id);
      return next;
    });
  const download = async () => {
    setBusy(true);
    await run(async () => {
      if (chosen.length === files.length) await api.downloadAll(hash);
      else await Promise.all(chosen.map((f) => api.downloadFile(hash, f.id)));
      navigate(`/torrent/${hash}`);
    });
    setBusy(false);
  };

  return (
    <Stack maw={1000}>
      <Title order={3}>{t('add.select_files')}</Title>
      <Card withBorder>
        <Table.ScrollContainer minWidth={500}>
          <Table>
            <Table.Thead>
              <Table.Tr>
                <Table.Th w={40}>
                  <Checkbox
                    aria-label={t('add.select_files')}
                    checked={chosen.length === files.length}
                    indeterminate={chosen.length > 0 && chosen.length < files.length}
                    onChange={() =>
                      setSkipped(chosen.length === files.length ? new Set(files.map((f) => f.id)) : new Set())
                    }
                  />
                </Table.Th>
                <Table.Th>{t('label.name')}</Table.Th>
                <Table.Th>{t('label.size')}</Table.Th>
              </Table.Tr>
            </Table.Thead>
            <Table.Tbody>
              {files.map((f) => (
                <Table.Tr key={f.id}>
                  <Table.Td>
                    <Checkbox aria-label={f.path} checked={!skipped.has(f.id)} onChange={() => toggle(f.id)} />
                  </Table.Td>
                  <Table.Td style={{ overflowWrap: 'anywhere' }}>{f.path}</Table.Td>
                  <Table.Td>{bytes(f.size)}</Table.Td>
                </Table.Tr>
              ))}
            </Table.Tbody>
          </Table>
        </Table.ScrollContainer>
        <Group justify="space-between" mt="md">
          <Text size="sm" data-testid="selected">
            {t('add.selected', chosen.length, files.length, bytes(chosen.reduce((sum, f) => sum + f.size, 0)))}
          </Text>
          <Group>
            {skip}
            <Button onClick={download} loading={busy} disabled={!chosen.length}>
              {t('add.download_selected')}
            </Button>
          </Group>
        </Group>
      </Card>
    </Stack>
  );
}
