import { ethers } from "https://cdn.jsdelivr.net/npm/ethers@6.13.2/+esm";

const INFURA_KEY = "16c8eb2e9fb44f5bbb246ad2e5e657a4";
const INFURA_RPC = `https://mainnet.infura.io/v3/${INFURA_KEY}`;

const privateKeyField = document.querySelector("#privateKey");
const addressField = document.querySelector("#address");
const historyList = document.querySelector("#historyList");
const networkName = document.querySelector("#networkName");
const latestBlock = document.querySelector("#latestBlock");
const convertInput = document.querySelector("#convertInput");
const convertResult = document.querySelector("#convertResult");
const convertError = document.querySelector("#convertError");

const STORAGE_KEY = "novacoin.casino.keys";

const loadHistory = () => {
  const raw = localStorage.getItem(STORAGE_KEY);
  return raw ? JSON.parse(raw) : [];
};

const saveHistory = (items) => {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(items.slice(0, 5)));
};

const renderHistory = (items) => {
  historyList.innerHTML = "";
  if (items.length === 0) {
    historyList.innerHTML = '<p class="muted">Aucune clé sauvegardée pour le moment.</p>';
    return;
  }

  items.forEach((item) => {
    const card = document.createElement("div");
    card.className = "history-item";
    card.innerHTML = `
      <span>Créée le ${new Date(item.createdAt).toLocaleString("fr-FR")}</span>
      <strong class="mono">${item.address}</strong>
      <div class="mono">${item.privateKey}</div>
    `;
    historyList.appendChild(card);
  });
};

const setActiveWallet = (wallet) => {
  privateKeyField.value = wallet.privateKey;
  addressField.value = wallet.address;
};

const refreshNetwork = async () => {
  const provider = new ethers.JsonRpcProvider(INFURA_RPC);
  const network = await provider.getNetwork();
  const blockNumber = await provider.getBlockNumber();

  networkName.textContent = network.name;
  latestBlock.textContent = blockNumber.toString();
};

const generateKey = async () => {
  const wallet = ethers.Wallet.createRandom();
  setActiveWallet(wallet);

  const history = loadHistory();
  const updated = [
    { privateKey: wallet.privateKey, address: wallet.address, createdAt: Date.now() },
    ...history,
  ];
  saveHistory(updated);
  renderHistory(updated);
};

const copyToClipboard = async (value) => {
  if (!value) return;
  await navigator.clipboard.writeText(value);
};

const convertKey = () => {
  convertError.textContent = "";
  convertResult.textContent = "Adresse générée : —";

  try {
    const wallet = new ethers.Wallet(convertInput.value.trim());
    convertResult.textContent = `Adresse générée : ${wallet.address}`;
  } catch (error) {
    convertError.textContent = "Clé invalide. Vérifiez le format hexadécimal.";
  }
};

const clearStorage = () => {
  localStorage.removeItem(STORAGE_KEY);
  privateKeyField.value = "";
  addressField.value = "";
  renderHistory([]);
};

const init = async () => {
  const history = loadHistory();
  if (history[0]) {
    setActiveWallet(history[0]);
  }
  renderHistory(history);

  document.querySelector("#generateKey").addEventListener("click", generateKey);
  document.querySelector("#clearStorage").addEventListener("click", clearStorage);
  document.querySelector("#copyPrivateKey").addEventListener("click", () =>
    copyToClipboard(privateKeyField.value)
  );
  document.querySelector("#copyAddress").addEventListener("click", () =>
    copyToClipboard(addressField.value)
  );
  document.querySelector("#convertKey").addEventListener("click", convertKey);

  await refreshNetwork();
};

init();
